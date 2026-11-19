# NixOS: nix-build default.nix
# Raven is fetched at the same pinned commit CMake uses; the hash is filled in by
# `nix-prefetch-url --unpack https://github.com/oscar-soirey/Raven/archive/<commit>.tar.gz`.
{ pkgs ? import <nixpkgs> {} }:
let
  ravenRev = "89197438a59edde8efc38c9b48223e6721ec1816";
  raven = pkgs.fetchFromGitHub {
    owner = "oscar-soirey";
    repo = "Raven";
    rev = ravenRev;
    hash = pkgs.lib.fakeHash;  # replace with the real hash before publishing
  };
in
pkgs.stdenv.mkDerivation {
  pname = "colibry-view";
  version = "0.2.0";
  src = ./.;
  nativeBuildInputs = [ pkgs.cmake ];
  buildInputs = [ pkgs.vulkan-loader pkgs.libx11 pkgs.xorg.libXi ];
  cmakeFlags = [
    "-DCOLIBRY_BUILD_TESTS=ON"
    "-DRAVEN_DIR=${raven}"
  ];
  doCheck = true;
  checkPhase = "ctest --output-on-failure";
  meta = with pkgs.lib; {
    description = "Colibry OS: Viewer for text, hex and images with a media player and background mode";
    license = licenses.mit;
    platforms = platforms.linux;
  };
}
