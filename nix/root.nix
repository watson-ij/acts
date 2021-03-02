{ pkgs, vdt } :
with pkgs; stdenv.mkDerivation rec {
  pname = "root";
  version = "6.22.06";
  src = fetchurl {
    url = "https://root.cern.ch/download/root_v${version}.source.tar.gz";
    sha256 = "0mqvj42nax0bmz8h83jjlwjm3xxjy1n0n10inc8csip9ly28fs64";
  };
  nativeBuildInputs = [ makeWrapper cmake pkgconfig ];
  buildInputs = [ gl2ps pcre python2 python3 zlib libxml2 llvm_5 lz4 lzma gsl xxHash zstd ftgl vdt cfitsio fftw tbb ]
                ++ (with xorg; [ libX11 libXpm libXft libXext libGLU libGL glew ]);
  propagatedBuildInputs = [ python2.pkgs.numpy python3.pkgs.numpy ];
  
  preConfigure = ''
    rm -rf builtins/*
    substituteInPlace cmake/modules/SearchInstalledSoftware.cmake \
      --replace 'set(lcgpackages ' '#set(lcgpackages '
    patchShebangs build/unix/
    substituteInPlace rootx/src/rootx.cxx --replace "gNoLogo = false" "gNoLogo = true"
  '';

  cmakeFlags = [
    "-DCMAKE_CXX_STANDARD=17"
    "-Drpath=ON"
    "-DCMAKE_INSTALL_LIBDIR=lib"
    "-DCMAKE_INSTALL_INCLUDEDIR=include"
    "-Dbuiltin_afterimage=ON"
    "-Dbuiltin_llvm=OFF"
    "-Dalien=OFF"
    "-Dbonjour=OFF"
    "-Dcastor=OFF"
    "-Dchirp=OFF"
    "-Dclad=OFF"
    "-Ddavix=OFF"
    "-Ddcache=OFF"
    "-Dfail-on-missing=ON"
    "-Dfftw3=ON"
    "-Dfitsio=ON"
    "-Dfortran=OFF"
    "-Dimt=ON"
    "-Dgfal=OFF"
    "-Dgviz=OFF"
    "-Dhdfs=OFF"
    "-Dkrb5=OFF"
    "-Dldap=OFF"
    "-Dmonalisa=OFF"
    "-Dminuit2=ON"
    "-Dmysql=OFF"
    "-Dodbc=OFF"
    "-Dopengl=ON"
    "-Doracle=OFF"
    "-Dpgsql=OFF"
    "-Dpythia6=OFF"
    "-Dpythia8=OFF"
    "-Drfio=OFF"
    "-Dsqlite=OFF"
    "-Dssl=OFF"
    "-Dvdt=ON"
    "-Dxml=ON"
    "-Dxrootd=OFF"
  ]
  ++ stdenv.lib.optional (stdenv.cc.libc != null) "-DC_INCLUDE_DIRS=${stdenv.lib.getDev stdenv.cc.libc}/include";
  enableParallelBuilding = true;
  postInstall = ''
    for prog in rootbrowse rootcp rooteventselector rootls rootmkdir rootmv rootprint rootrm rootslimtree; do
      wrapProgram "$out/bin/$prog" \
        --prefix PYTHONPATH : "$out/lib"
    done
  '';
  setupHook = ./setup-hook.sh;
}
