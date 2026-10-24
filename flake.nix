{
  description = "libTAS appimage with with GPU enabled savestates";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    # libTAS from nixpkgs-unstable would need glibc 2.44 (libm symbol versions).
    nixpkgsLibtas.url = "github:NixOS/nixpkgs/nixos-24.05";
    flake-utils.url = "github:numtide/flake-utils";

    mesaGpu = {
      url = "git+https://gitlab.freedesktop.org/jakobhellermann/mesa?ref=dev&shallow=1";
      inputs.nixpkgs.follows = "nixpkgs";
    };
    gfxstream = {
      url = "github:jakobhellermann/gfxstream/dev";
      inputs.nixpkgs.follows = "nixpkgs";
    };
    kumquat = {
      url = "github:jakobhellermann/rutabaga_gfx/dev";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.gfxstream.follows = "gfxstream";
    };
  };

  outputs =
    {
      self,
      nixpkgs,
      nixpkgsLibtas,
      flake-utils,
      mesaGpu,
      kumquat,
      ...
    }:
    flake-utils.lib.eachSystem [ "x86_64-linux" ] (
      system:
      let
        pkgs = import nixpkgs { inherit system; };
        pkgsLibtas = import nixpkgsLibtas { inherit system; };

        libtasUnwrapped = pkgsLibtas.libtas.overrideAttrs (old: {
          version = "gpu-${self.shortRev or self.dirtyShortRev or "dirty"}";
          src = self;
          patches = [ ];
          # An interim build reports its commit, which the sandbox has no git for
          configureFlags = [ ];
          buildInputs = old.buildInputs ++ [ pkgsLibtas.libcap ];
          # The AppImage sets these paths through qt.conf and AppRun instead.
          dontWrapQtApps = true;
          postFixup = "";
          # doesnt work in nix sandbox / appimage
          postPatch = (old.postPatch or "") + ''
            substituteInPlace src/program/Makefile.am \
              --replace-fail 'if command -v setcap' 'if false'
          '';
          # libtas.so runs inside the game, which may bring its own libstdc++.
          env = (old.env or { }) // {
            LDFLAGS = "-static-libstdc++ -static-libgcc";
            LIBTAS_GIT_COMMIT = self.shortRev or self.dirtyShortRev or "dirty";
            LIBTAS_GIT_DATE =
              let d = self.lastModifiedDate; in
              "${builtins.substring 0 4 d}-${builtins.substring 4 2 d}-${builtins.substring 6 2 d}";
          };
        });

        kumquatPackage = kumquat.packages.${system}.default;
        mesaGpuPackage = mesaGpu.packages.${system}.gfxstream;
        gfxstreamIcd = "${mesaGpuPackage}/share/vulkan/icd.d/gfxstream_vk_icd.x86_64.json";

        libtas = libtasUnwrapped.overrideAttrs (old: {
          dontWrapQtApps = false;
          postFixup = pkgsLibtas.libtas.postFixup + ''
            wrapProgram $out/bin/libTAS \
              --set-default LIBTAS_KUMQUAT_BIN ${kumquatPackage}/bin/kumquat \
              --set-default LIBTAS_GFXSTREAM_ICD ${gfxstreamIcd} \
              --set QT_QPA_PLATFORM xcb
          '';
        });
        qtbase = pkgsLibtas.libsForQt5.qtbase;

        appimageRuntime = pkgs.fetchurl {
          url = "https://github.com/AppImage/type2-runtime/releases/download/20251108/runtime-x86_64";
          sha256 = "0396xi3km1yd0z4329lbjxmv82ddc40gd0x8hca0ylcj7i28pjig";
        };

        # gfxstream from nixpkgs-unstable uses GNU2 TLS descriptors (GLIBC_ABI_GNU2_TLS)
        glibcFloor = "2.43";

        appimage = pkgs.stdenv.mkDerivation {
          pname = "libtas-gpu-appimage";
          inherit (libtas) version;

          dontUnpack = true;
          dontStrip = true;

          nativeBuildInputs = with pkgs; [
            patchelf
            binutils
            squashfsTools
            jq
            file
          ];

          buildCommand = ''
            appdir=$PWD/AppDir
            bindir=$appdir/usr/bin
            libdir=$appdir/usr/lib
            plugindir=$appdir/usr/plugins
            kqdir=$appdir/usr/kumquat
            mkdir -p $bindir $libdir $plugindir/platforms $kqdir/bin $kqdir/lib $appdir/usr/share/vulkan/icd.d

            install -m755 ${libtasUnwrapped}/bin/libTAS $bindir/libTAS
            install -m755 ${libtasUnwrapped}/lib/libtas.so $bindir/libtas.so
            install -m755 ${qtbase.bin}/${qtbase.qtPluginPrefix}/platforms/libqxcb.so $plugindir/platforms/
            printf '[Paths]\nPrefix = ..\n' > $bindir/qt.conf

            install -m755 ${kumquatPackage}/bin/.kumquat-wrapped $kqdir/bin/kumquat
            # kumquat's gfxstream backend dlopens libvulkan.so.1; the game loads the guest ICD.
            install -m755 ${pkgs.vulkan-loader}/lib/libvulkan.so.1 $kqdir/lib/libvulkan.so.1
            install -m755 ${mesaGpuPackage}/lib/libvulkan_gfxstream.so $kqdir/lib/libvulkan_gfxstream.so

            # Libraries the libTAS GUI takes from the host: the X server, GL and font
            # stacks must match the system, and libtas.so in the game links them too.
            host_lib() {
              case "$1" in
                libX11.so.6 | libX11-xcb.so.1 | libxcb.so.1 | libXau.so.6 | libXdmcp.so.6 | \
                libGL.so.1 | libGLX.so.0 | libGLdispatch.so.0 | libOpenGL.so.0 | libEGL.so.1 | \
                libdrm.so.2 | libgbm.so.1 | libfontconfig.so.1 | libfreetype.so.6 | \
                libasound.so.2 | libudev.so.1 | libsystemd.so.0) return 0 ;;
              esac
              return 1
            }

            # Copies the Nix dependencies of the ELFs in $1 into $2 until nothing is missing.
            bundle() {
              local elfdirs=$1 dest=$2 use_host=$3 changed=1
              while [ "$changed" -ne 0 ]; do
                changed=0
                for elf in $(find $elfdirs -type f); do
                  file -b "$elf" | grep -q ELF || continue
                  deps=$(ldd "$elf")
                  while read -r name arrow path rest; do
                    [ "$arrow" = "=>" ] || continue
                    case "$path" in
                      not)
                        echo "unresolved dependency of $elf: $name" >&2
                        exit 1
                        ;;
                      /nix/store/*-glibc-*) continue ;;
                      /nix/store/*) ;;
                      *) continue ;;
                    esac
                    [ "$use_host" = 1 ] && host_lib "$name" && continue
                    [ -e "$dest/$name" ] && continue
                    cp -L "$path" "$dest/$name"
                    changed=1
                  done <<< "$deps"
                done
              done
            }

            # libtas.so only needs host libraries.
            bundle "$bindir/libTAS $plugindir $libdir" $libdir 1
            bundle "$kqdir" $kqdir/lib 0
            chmod -R u+w "$appdir"

            for elf in $bindir/libTAS $kqdir/bin/kumquat; do
              patchelf --set-interpreter /lib64/ld-linux-x86-64.so.2 "$elf"
            done
            # Steam's LD_LIBRARY_PATH carries the host /usr/lib, which outranks RUNPATH but
            # not DT_RPATH. Objects loaded into the game keep RUNPATH and defer to it.
            patchelf --force-rpath --set-rpath '$ORIGIN/../lib' $bindir/libTAS
            patchelf --remove-rpath $bindir/libtas.so
            patchelf --force-rpath --set-rpath '$ORIGIN/../../lib' $plugindir/platforms/libqxcb.so
            for elf in $(find $libdir -maxdepth 1 -type f); do
              patchelf --force-rpath --set-rpath '$ORIGIN' "$elf"
            done
            patchelf --force-rpath --set-rpath '$ORIGIN/../lib' $kqdir/bin/kumquat
            for elf in $(find $kqdir/lib -maxdepth 1 -type f); do
              case "$elf" in
                */libvulkan_gfxstream.so) patchelf --set-rpath '$ORIGIN' "$elf" ;;
                *) patchelf --force-rpath --set-rpath '$ORIGIN' "$elf" ;;
              esac
            done

            # glibc compatibility check
            for elf in $(find $appdir/usr -type f); do
              file -b "$elf" | grep -q ELF || continue
              info=$(readelf -W -V "$elf")
              needs=$(awk '/Name: GLIBC_/ && !/Flags: WEAK/ {print $3}' <<< "$info" | sort -Vu)
              for need in $needs; do
                case "$need" in
                  GLIBC_[0-9]*) v=''${need#GLIBC_} ;;
                  GLIBC_ABI_GNU2_TLS) v=2.43 ;;
                  *) v= ;;
                esac
                [ -n "$v" ] && [ "$(printf '%s\n${glibcFloor}\n' "$v" | sort -V | tail -1)" = "${glibcFloor}" ] && continue
                echo "glibc floor ${glibcFloor} violated: $elf needs $need" >&2
                exit 1
              done
            done

            api_version=$(jq -r .ICD.api_version ${gfxstreamIcd})
            cat > $appdir/usr/share/vulkan/icd.d/gfxstream_vk_icd.x86_64.json <<JSON
            {
              "file_format_version": "1.0.1",
              "ICD": {
                "api_version": "$api_version",
                "library_path": "../../../kumquat/lib/libvulkan_gfxstream.so"
              }
            }
            JSON

            cat > $appdir/AppRun <<'APPRUN'
            #!/bin/sh
            here=$(dirname "$(readlink -f "$0")")
            export LIBTAS_KUMQUAT_BIN="''${LIBTAS_KUMQUAT_BIN:-$here/usr/kumquat/bin/kumquat}"
            export LIBTAS_GFXSTREAM_ICD="''${LIBTAS_GFXSTREAM_ICD:-$here/usr/share/vulkan/icd.d/gfxstream_vk_icd.x86_64.json}"
            # libTAS does not support Wayland
            export QT_QPA_PLATFORM=xcb
            exec "$here/usr/bin/libTAS" "$@"
            APPRUN
            chmod 755 $appdir/AppRun
            cp ${self}/libTAS.svg $appdir/libTAS.svg
            ln -s libTAS.svg $appdir/.DirIcon
            cp ${self}/libTAS.desktop $appdir/libTAS.desktop

            mksquashfs \
              "$appdir/AppRun" "$appdir/usr" "$appdir/libTAS.desktop" \
              "$appdir/libTAS.svg" "$appdir/.DirIcon" \
              squashfs.img \
              -all-root -noappend -no-recovery -comp zstd
            cat ${appimageRuntime} squashfs.img > $out
            chmod 755 $out
          '';
        };
      in
      {
        packages = {
          inherit libtas appimage;
        };

        # Builds build/ and runs devtest/run (under steam-run, for libXi and /usr/share/vulkan).
        # Uses the host's nixpkgs, not nixpkgsLibtas: the game preloads libtas.so, whose
        # RUNPATH would put an older libm next to the host Mesa.
        devShells.default = pkgs.mkShell {
          inputsFrom = [ pkgs.libtas ];
          packages = with pkgs; [
            libcap
            libGL
            libx11
            vulkan-headers
            vulkan-loader
            python3
            autoconf
            automake
            xvfb-run
            elfutils
          ];
          LIBTAS_GPU_KUMQUAT_BIN = "${kumquatPackage}/bin/kumquat";
          LIBTAS_GPU_ICD = gfxstreamIcd;
          # build/ is not wrapped by wrapQtApps.
          QT_PLUGIN_PATH = "${pkgs.libsForQt5.qtbase.bin}/${pkgs.libsForQt5.qtbase.qtPluginPrefix}";
          # Validation, and api_dump for devtest/run --api-dump.
          VK_ADD_LAYER_PATH = "${pkgs.vulkan-validation-layers}/share/vulkan/explicit_layer.d:${pkgs.vulkan-tools-lunarg}/share/vulkan/explicit_layer.d";
          shellHook = ''
            # Profile library paths would link libraries built for another glibc.
            unset LIBRARY_PATH CPATH
            # devtest/run gives each session a private /tmp, hiding nix develop's TMPDIR.
            export TMPDIR=/tmp
          '';
        };
      }
    );
}
