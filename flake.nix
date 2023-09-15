{
  description = "Clio";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/release-23.05";
    flake-utils.url = "github:numtide/flake-utils";
    nixpkgs_for_fmt.url = "github:NixOS/nixpkgs/50a7139fbd1acd4a3d4cfa695e694c529dd26f3a";
    # grpc 1.51.1
    nixpkgs_for_grpc.url = "github:NixOS/nixpkgs/8ad5e8132c5dcf977e308e7bf5517cc6cc0bf7d8";
    nixpkgs_for_lz4.url = "github:NixOS/nixpkgs/7cf5ccf1cdb2ba5f08f0ac29fc3d04b0b59a07e4";
  };

  outputs = { self, nixpkgs, nixpkgs_for_fmt, nixpkgs_for_grpc, nixpkgs_for_lz4, flake-utils }:
    flake-utils.lib.eachDefaultSystem (
    system:
        let
            pkgs = import nixpkgs {
                inherit system;
                config.permittedInsecurePackages = [
                    "openssl-1.1.1v"
                ];
                overlays = [
                    (self: super: {
                        fmt = nixpkgs_for_fmt.legacyPackages.${system}.fmt;
                        grpc = nixpkgs_for_grpc.legacyPackages.${system}.grpc;
                        lz4 = nixpkgs_for_lz4.legacyPackages.${system}.lz4;
                        soci = super.soci.overrideAttrs {
                             cmakeFlags = [ "-DSOCI_STATIC=OFF" "-DCMAKE_CXX_STANDARD=11" "-DSOCI_TESTS=off" "-DWITH_POSTGRESQL=off"];
                              buildInputs = [
                                super.sqlite
                                super.boost182
                              ];
                        };
                    })
            ];
            };
        in
        {
          devShells.default = pkgs.callPackage ./external/nix/shell.nix { inherit pkgs; };
        }
      );
}
