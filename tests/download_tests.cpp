#include "core/project/minecraft_download.hpp"
#include "core/util/debug.hpp"
#include "core/util/zip_writer.hpp"
#include "test_utils.hpp"

#include <filesystem>
#include <string>

using namespace enchantment_tweaks;
using namespace enchantment_tweaks::project;
namespace fs = std::filesystem;

int main() {
  test_util::Suite suite;

  suite.run("sha1 matches the standard test vectors", [] {
    test_util::check(sha1Hex({}) == "da39a3ee5e6b4b0d3255bfef95601890afd80709",
                     "empty input");
    const std::string abc = "abc";
    test_util::check(sha1Hex({abc.begin(), abc.end()}) ==
                         "a9993e364706816aba3e25717850c26c9cd0d89d",
                     "abc");
    const std::string two =
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    test_util::check(sha1Hex({two.begin(), two.end()}) ==
                         "84983e441c3bd26ebaae4aa1f95129e5e54670f1",
                     "two-block message");
  });

  suite.run("version manifest lists versions and skips broken entries", [] {
    const auto m =
        parseVersionManifest(R"({"latest":{"release":"1.21.4"},"versions":[
      {"id":"1.21.4","type":"release","url":"https://example.invalid/1.21.4.json"},
      {"id":"24w01a","type":"snapshot","url":"https://example.invalid/24w01a.json"},
      {"id":"no-url","type":"release"}]})");
    test_util::check(m.error.empty(), "manifest failed: " + m.error);
    test_util::check(m.versions.size() == 2,
                     "expected 2 usable entries, got " +
                         std::to_string(m.versions.size()));
    if (m.versions.size() == 2) {
      test_util::check(m.versions[0].id == "1.21.4" &&
                           m.versions[0].type == "release",
                       "first entry wrong");
    }
  });

  suite.run("a broken manifest is an error", [] {
    test_util::check(!parseVersionManifest("{\"nope\":1}").error.empty(),
                     "wrong shape accepted");
    test_util::check(!parseVersionManifest("not json").error.empty(),
                     "garbage accepted");
  });

  suite.run("client download is read from version metadata", [] {
    const auto r = parseClientDownload(R"({"id":"1.21.4","downloads":{"client":{
      "sha1":"0123456789abcdef0123456789abcdef01234567","size":123,
      "url":"https://example.invalid/client.jar"}}})");
    test_util::check(r.download.has_value(), "download not read: " + r.error);
    if (r.download) {
      test_util::check(r.download->size == 123 &&
                           r.download->url ==
                               "https://example.invalid/client.jar",
                       "fields wrong");
    }
  });

  suite.run("metadata without a client, or with a bad hash, is rejected", [] {
    test_util::check(
        !parseClientDownload("{\"downloads\":{}}").download.has_value(),
        "no client accepted");
    test_util::check(
        !parseClientDownload(
             R"({"downloads":{"client":{"sha1":"short","url":"x"}}})")
             .download.has_value(),
        "short hash accepted");
  });

  suite.run("resource format is read from a JAR without extracting it", [] {
    const fs::path jar = fs::temp_directory_path() / "et_version_probe.jar";
    zip::ZipWriter zw;
    zw.addFile(
        "version.json",
        std::string(
            R"({"id":"1.21.4","pack_version":{"resource":46,"data":61}})"));
    zw.addFile("assets/minecraft/items/bow.json", std::string("{}"));
    zw.write(jar);
    test_util::check(resourceFormatOfJar(jar) == 46, "format not read");
    fs::remove(jar);
  });

  suite.run("26.x JARs report resource_major", [] {
    const fs::path jar = fs::temp_directory_path() / "et_version_probe26.jar";
    zip::ZipWriter zw;
    zw.addFile(
        "version.json",
        std::string(
            R"({"id":"26.1","pack_version":{"resource_major":84,"resource_minor":0}})"));
    zw.write(jar);
    test_util::check(resourceFormatOfJar(jar) == 84, "resource_major not read");
    fs::remove(jar);
  });

  suite.run("a JAR without version.json has no format", [] {
    test_util::check(!resourceFormatOfJar("/no/such.jar").has_value(),
                     "missing file gave a format");
  });

  return suite.finish();
}
