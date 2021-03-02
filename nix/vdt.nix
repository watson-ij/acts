{ pkgs, src } : with pkgs; stdenv.mkDerivation {
  src = fetchurl {
    url = "https://github.com/dpiparo/vdt/archive/v0.4.3.tar.gz";
    sha256 = "1qdc10p4j6jl0as3a8pfvrygxdry2x6izxm8clmihp5v5rhp8mkh";
  };
  pname = "vdt";
  version = "v0.4.3";
  nativeBuildInputs = [cmake];
  buildInputs = [ python2 ];
}
