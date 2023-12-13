#include "requests.h"
#include "hurl.h"
#include "config.h"
#include "state.h"
#include "spdlog/spdlog.h"

#include <cmath>
#include <time.h>
#include <sstream>
#include <iomanip>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <experimental/filesystem>

namespace fs = std::experimental::filesystem;

namespace KUtils {
  bool is_running_local() {
    Config *conf = Config::get_instance();
    std::string df_host = conf->get<std::string>(conf->df() + "moonraker_host");
    return df_host == "localhost" || df_host == "127.0.0.1";
  }

  std::string get_root_path(const std::string root_name) {
    auto roots = State::get_instance()->get_data("/roots"_json_pointer);
    json filtered;
    std::copy_if(roots.begin(), roots.end(),
		 std::back_inserter(filtered), [&root_name](const json& item) {
		   return item.contains("name") && item["name"] == root_name;
		 });

    spdlog::trace("roots {}, filtered {}", roots.dump(), filtered.dump());
    if (!filtered.empty()) {
      return filtered["/0/path"_json_pointer];
    }

    return "";
  }

  std::string get_thumbnail(const std::string &gcode_file, json &j) {
    auto &thumbs = j["/result/thumbnails"_json_pointer];
    if (!thumbs.is_null() && !thumbs.empty()) {
      // assume square, look for closest to 300x300
      uint32_t closest_index = 0;
      int closest = std::abs(300 - thumbs.at(0)["width"].template get<int>());
      for (int i = 0; i < thumbs.size(); i++) {
	int cur_diff = std::abs(300 - thumbs.at(i)["width"].template get<int>());
	if (cur_diff < closest) {
	  closest = cur_diff;
	  closest_index = i;
	}
      }

      auto &thumb = thumbs.at(closest_index);
      spdlog::trace("using thumb at index {}, {}", closest_index, thumb.dump());

      // metadata thumbnail paths are relative to the current gcode file directory
      std::string relative_path = thumb["relative_path"].template get<std::string>();
      size_t found = gcode_file.find_last_of("/\\");
      if (found != std::string::npos) {
	relative_path = gcode_file.substr(0, found + 1) + relative_path;
      }

      Config *conf = Config::get_instance();
      std::string df_host = conf->get<std::string>(conf->df() + "moonraker_host");
      std::string fname = relative_path.substr(relative_path.find_last_of("/\\") + 1);
      std::string fullpath = fmt::format("{}/{}", conf->get<std::string>("/thumbnail_path"), fname);
    
      // download thumbnail
      if (is_running_local()) {
	spdlog::debug("running locally, skipping thumbnail downloads");
	auto gcode_root = get_root_path("gcodes");
	fullpath = fmt::format("{}/{}", gcode_root, relative_path);
      } else {
	std::string thumb_url = fmt::format("http://{}:{}/server/files/gcodes/{}",
					    df_host,
					    conf->get<uint32_t>(conf->df() + "moonraker_port"),
					    HUrl::escape(relative_path));


	// threadpool this
	spdlog::debug("thumb url {}", thumb_url);
	auto size = requests::downloadFile(thumb_url.c_str(), fullpath.c_str());
	spdlog::trace("downloaded size {}", size);
      }

      return fullpath;    
    }

    return "";
  }

  std::string download_file(const std::string &root,
			    const std::string &fname,
			    const std::string &dest) {

    auto filename = fs::path(fname).filename();
    auto dest_fullpath = fs::path(dest) / filename;

    spdlog::trace("root {}, fname {}, base filename {}, dest_fp {}", root, fname,
		  filename.string(), dest_fullpath.string());
    Config *conf = Config::get_instance();
    std::string df_host = conf->get<std::string>(conf->df() + "moonraker_host");

    std::string file_url = fmt::format("http://{}:{}/server/files/{}/{}",
					df_host,
					conf->get<uint32_t>(conf->df() + "moonraker_port"),
					root,
					HUrl::escape(fname));
    // threadpool this
    spdlog::debug("file url {}", file_url);
    auto size = requests::downloadFile(file_url.c_str(), dest_fullpath.c_str());
    spdlog::trace("downloaded file size {}", size);

    return dest_fullpath.string();
  }

  std::vector<std::string> get_interfaces() {
    struct ifaddrs *addrs;
    getifaddrs(&addrs);

    std::vector<std::string> ifaces;
    for (struct ifaddrs *addr = addrs; addr != nullptr; addr = addr->ifa_next) {
        if (addr->ifa_addr && addr->ifa_addr->sa_family == AF_PACKET) {
	  ifaces.push_back(addr->ifa_name);
        }
    }

    freeifaddrs(addrs);
    return ifaces;
  }

  std::string interface_ip(const std::string &interface) {
    int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);

    struct ifreq ifr{};
    strcpy(ifr.ifr_name, interface.c_str());
    ioctl(fd, SIOCGIFADDR, &ifr);
    close(fd);

    char ip[INET_ADDRSTRLEN];
    strcpy(ip, inet_ntoa(((sockaddr_in *) &ifr.ifr_addr)->sin_addr));
    return ip;
  }

  template <typename Out>
  void split(const std::string &s, char delim, Out result) {
    std::istringstream iss(s);
    std::string item;
    while (std::getline(iss, item, delim)) {
        *result++ = item;
    }
  }

  std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    split(s, delim, std::back_inserter(elems));
    return elems;
  }

  std::string get_obj_name(const std::string &id) {
    size_t pos = id.find_last_of(' ');
    return id.substr(pos + 1);
  }

  std::string to_title(std::string s) {
    bool last = true;
    for (char& c : s) {
      c = last ? std::toupper(c) : std::tolower(c);
      if (c == '_') {
	c = ' ';
      }

      last = std::isspace(c);
    }
    return s;
  }


  std::string eta_string(int64_t s) {
    time_t seconds (s);
    tm p;
    gmtime_r (&seconds, &p);

    std::ostringstream os;

    if (p.tm_yday > 0)
      os << p.tm_yday << "d ";

    if (p.tm_hour > 0)
      os << p.tm_hour << "h ";

    if (p.tm_min > 0)
      os << p.tm_min << "m ";

    os << p.tm_sec << "s";
    
    return os.str();
  }

  size_t bytes_to_mb(size_t s) {
    return s / 1024 / 1024;
  }
}
