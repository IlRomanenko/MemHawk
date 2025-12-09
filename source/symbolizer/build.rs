fn main() {
    let proto_dir = "../protos";
    let proto_file = "snapshot.proto";

    println!("cargo:rerun-if-changed={}", proto_dir);
    println!("cargo:rerun-if-changed=build.rs");

    prost_build::Config::new()
        .out_dir("protos")
        .type_attribute(
            ".",
            "#[derive(bitcode::Encode, bitcode::Decode)]", //  #[serde(rename_all = \"snake_case\")]
        )
        .compile_protos(&[format!("{}/{}", proto_dir, proto_file)], &[proto_dir])
        .unwrap();
}
