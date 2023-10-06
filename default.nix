{ lib
, gcc13
, cmake
, protobuf
, protobufc
, yaml-cpp }:

gcc13.stdenv.mkDerivation rec {
  pname = "tetris-server";
  version = "0.1.0";
  
  src = ./.;

  nativeBuildInputs = [ cmake ];
  buildInputs = [ 
    protobuf
    yaml-cpp 
  ];

  cmakeFlags = [
    "-DCMAKE_EXPORT_COMPILE_COMMANDS=1"
  ];

  meta = with lib; {
    description = ''
      TETRiS;
    '';
    licencse = licenses.mit;
    platforms = with platforms; linux ++ darwin;
  };
}
