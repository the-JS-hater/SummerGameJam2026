with import <nixpkgs> {};
mkShell {
  buildInputs = [ libx11 ];
}