import os
from conan import ConanFile
from conan.tools.files import get, copy, collect_libs, rename
from conan.tools.gnu import Autotools, AutotoolsToolchain, PkgConfigDeps
from conan.tools.env import Environment, VirtualBuildEnv
from conan.tools.microsoft import (
    VCVars,
    MSBuild,
    MSBuildToolchain,
    msvs_toolset,
)

class libnodeConan(ConanFile):
    name = "libnode"
    license = "MIT"
    url = "https://nodejs.org"
    description = (
        "Node.js is an open-source, cross-platform JavaScript runtime environment"
    )
    settings = "os", "compiler", "build_type", "arch"

    def __add_shared(self, pkg_name, libname):
        libs = self.dependencies[pkg_name].cpp_info.libs
        libs += self.dependencies[pkg_name].cpp_info.system_libs
        for c in self.dependencies[pkg_name].cpp_info.components.values():
            libs += c.libs
            libs += c.system_libs
        includes = ",".join(self.dependencies[pkg_name].cpp_info.includedirs)
        libnames = ",".join(libs)
        libpath = ",".join(self.dependencies[pkg_name].cpp_info.libdirs)
        return [
            f"--shared-{libname}",
            f"--shared-{libname}-includes={includes}",
            f"--shared-{libname}-libname={libnames}",
            f"--shared-{libname}-libpath={libpath}",
        ]

    def build_requirements(self):
        self.tool_requires("nasm/2.15.05")

    def requirements(self):
        self.requires("brotli/[>1.0 <1.2]")
        self.requires("llhttp/[>6.0 <7.0]")
        # self.requires("libnghttp2/[>1.50 <1.60]")
        # self.requires("libuv/[>1.40 <1.50]")
        self.requires("openssl/1.1.1w")
        self.requires("zlib/[>1.0 <1.4]")

    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)

    def generate(self):
        if self.settings.os == "Windows":
            toolset = msvs_toolset(self)
            msvs_version = {"192": "2019", "193": "2022"}.get(
                str(self.settings.compiler.version)
            )
            node_build_env = Environment()
            node_build_env.define("GYP_MSVS_VERSION", msvs_version)
            node_build_env.define("PLATFORM_TOOLSET", toolset)
            envvars = node_build_env.vars(self)
            envvars.save_script("node_build_env")
            vbe = VirtualBuildEnv(self)
            vbe.generate()
            tc = MSBuildToolchain(self)
            tc.configuration = str(self.settings.build_type)
            tc.generate()
            tc = VCVars(self)
            tc.generate()
        else:
            tc = AutotoolsToolchain(self)
            tc.generate()
            pc = PkgConfigDeps(self)
            pc.generate()
            node_build_env = Environment()
            node_build_env.define("PKG_CONFIG_PATH", self.build_folder)
            envvars = node_build_env.vars(self)
            envvars.save_script("node_build_env")
            rename(self, "libllhttp.pc", "http_parser.pc")

    def build(self):
        args = [
            # "--ninja",
            "--shared",
            "--without-npm",
            "--without-corepack",
            "--without-intl",
            "--v8-enable-object-print",
            "--prefix=%s" % self.package_folder,
        ]

        # TODO Fix building with these libs
        # args += self.__add_shared("", "cares")
        # args += self.__add_shared("", "nghttp3")
        # args += self.__add_shared("", "ngtcp2")
        # args += self.__add_shared("libnghttp2", "nghttp2")
        # args += self.__add_shared("libuv", "libuv")
        args += self.__add_shared("brotli", "brotli")
        args += self.__add_shared("llhttp", "http-parser")
        args += self.__add_shared("openssl", "openssl")
        args += self.__add_shared("zlib", "zlib")

        args.append("" if self.settings.build_type == "Release" else "--debug")
        args.append("--dest-cpu=%s" % self.settings.arch)

        if self.settings.os == "Linux":
            args.append("--gdb")

        self.run(
            "python configure.py %s" % (" ".join(args)), env=["node_build_env"]
        )
        if self.settings.os == "Windows":
            # self.run("ninja libnode", env=["node_build_env"])
            msbuild = MSBuild(self)
            msbuild.build("node.sln", targets=["libnode"])
        else:
            autotools = Autotools(self)
            autotools.make(args=["-C out", "BUILDTYPE=%s" % self.settings.build_type], target="libnode")

    def package(self):
        if self.settings.os == "Windows":
            self.run(
                "set HEADERS_ONLY=1 && python ./tools/install.py install %s\\ \\"
                % self.package_folder
            )
            copy(
                self,
                "*.h",
                os.path.join(
                    self.source_folder, "deps", "v8", "include", "libplatform"
                ),
                os.path.join(self.package_folder, "include", "libplatform"),
                keep_path=False,
            )
            copy(
                self,
                "*.h",
                os.path.join(self.source_folder, "deps", "v8", "include", "cppgc"),
                os.path.join(self.package_folder, "include", "cppgc"),
                keep_path=False,
            )
            copy(
                self,
                "libnode.lib",
                os.path.join(self.source_folder, "out", str(self.settings.build_type)),
                os.path.join(self.package_folder, "lib"),
                keep_path=False,
            )
            copy(
                self,
                "v8_libplatform.lib",
                os.path.join(
                    self.source_folder, "out", str(self.settings.build_type), "lib"
                ),
                os.path.join(self.package_folder, "lib"),
                keep_path=False,
            )
            copy(
                self,
                "*.dll",
                os.path.join(self.source_folder, "out"),
                os.path.join(self.package_folder, "bin"),
                keep_path=False,
            )
        else:
            self.run(
                "export HEADERS_ONLY=1 && python ./tools/install.py install %s/ /"
                % self.package_folder
            )
            copy(
                self,
                "*.h",
                os.path.join(
                    self.source_folder, "deps", "v8", "include", "libplatform"
                ),
                os.path.join(self.package_folder, "include", "libplatform"),
                keep_path=False,
            )
            copy(
                self,
                "*.h",
                os.path.join(self.source_folder, "deps", "v8", "include", "cppgc"),
                os.path.join(self.package_folder, "include", "cppgc"),
                keep_path=False,
            )
            copy(
                self,
                "libnode.so*",
                os.path.join(self.source_folder, "out", str(self.settings.build_type)),
                os.path.join(self.package_folder, "lib"),
                keep_path=False
            )

    def package_info(self):
        self.cpp_info.includedirs = ["include", "include/node"]
        self.cpp_info.libs = collect_libs(self)