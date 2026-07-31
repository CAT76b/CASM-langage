use std::fs;
use std::env;

fn main() {
    let args: Vec<String> = env::args().collect();
    let filename = args.get(1).expect("Usage: newdisk [FILENAME]");
    let mut txt = String::new();
    let mut it = 1;
    loop {
        txt += "\n";
        println!("iternation n{it}");
        if it == 1999999 {break} else {it+=1};
    }
    println!("Finished");
    fs::write(filename, txt).expect("coudln't write to file");
}
