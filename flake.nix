{
  description = "A very basic flake";
  # Provides abstraction to boiler-code when specifying multi-platform outputs.
  inputs.flake-utils.url = "github:numtide/flake-utils";
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system: let
      pkgs = nixpkgs.legacyPackages.${system};
    in {
      devShell = let
        vdt = pkgs.callPackage ./nix/vdt.nix {};
        myroot = pkgs.callPackage ./nix/root.nix {inherit vdt;};
        # https://github.com/NixOS/nixpkgs/issues/63104
        myboost = pkgs.symlinkJoin {
          name = pkgs.boost171.name + "+cmake";
          paths = [ pkgs.boost171 pkgs.boost171.dev ];
        };
        in pkgs.mkShell {
        TBB_DIR=pkgs.tbb;
        nativeBuildInputs = with pkgs; [
          myboost eigen gcc cmake hepmc3 tbb  pythia geant4 myroot
          (python3.withPackages (ps: with ps;
            [jupyter numpy matplotlib pandas numba six onnx]))
        ];
      };
    });
}
