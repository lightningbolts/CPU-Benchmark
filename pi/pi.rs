use std::fs::File;
use std::io::prelude::*;
use std::process::Command;
use std::time::Instant;
use rand::Rng;
use rayon::prelude::*;
use mongodb::{Client, options::ServerApi};

fn monte_carlo_pi(points: u64) -> u64 {
    let mut inside_circle = 0;
    for _ in 0..points {
        let x: f64 = rand::thread_rng().gen();
        let y: f64 = rand::thread_rng().gen();
        let distance = x.powi(2) + y.powi(2);
        if distance <= 1.0 {
            inside_circle += 1;
        }
    }
    inside_circle
}

fn calculate_pi_with_multiprocessing(digits: u64, processes: usize) -> f64 {
    let total_points = digits;
    let points_per_process = total_points / processes as u64;
    let results: Vec<u64> = (0..processes).into_par_iter().map(|_| monte_carlo_pi(points_per_process)).collect();
    let total_inside_circle: u64 = results.iter().sum();
    let pi_estimate = (total_inside_circle as f64 / total_points as f64) * 4.0;
    pi_estimate
}

fn execution_time_for_multiprocessing(digits: u64, processes: usize) -> f64 {
    let start_time = Instant::now();
    calculate_pi_with_multiprocessing(digits, processes);
    let end_time = Instant::now();
    let execution_time = end_time.duration_since(start_time).as_secs_f64();
    execution_time
}

fn calculate_multi_core_score(digits: u64, execution_time: f64) -> u64 {
    let multi_core_score = (digits as f64 / execution_time) / 3333.0;
    multi_core_score.round() as u64
}

fn calculate_gpu_score(digits: u64, execution_time: f64) -> u64 {
    let gpu_score = (digits as f64 / execution_time) / 3333.0;
    gpu_score.round() as u64
}

fn display_data_in_txt() {
    let benchmark_data = std::fs::read_to_string("pi_benchmark.json").unwrap();
    let mut file = File::create("pi_benchmark.txt").unwrap();
    file.write_all(b"CPU Model                                          | Execution time (single core) | Execution time (multi core) | Single core score | Multi core score | Speedup | Efficiency | CPU utilization\n\n").unwrap();
    for line in benchmark_data.lines() {
        let data: serde_json::Value = serde_json::from_str(line).unwrap();
        let cpu_model = data["cpu_model"].as_str().unwrap();
        let execution_time_single_core = data["execution_time_single_core"].as_f64().unwrap();
        let execution_time_multi_core = data["execution_time_multi_core"].as_f64().unwrap();
        let single_core_score = data["single_core_score"].as_u64().unwrap();
        let multi_core_score = data["multi_core_score"].as_u64().unwrap();
        let speedup = data["speedup"].as_f64().unwrap();
        let efficiency = data["efficiency"].as_f64().unwrap();
        let cpu_utilization = data["cpu_utilization"].as_f64().unwrap();
        let line = format!("{:<51}| {:<29}| {:<28}| {:<18}| {:<17}| {:<8}| {:<11}| {}%\n", cpu_model, execution_time_single_core, execution_time_multi_core, single_core_score, multi_core_score, speedup, efficiency, cpu_utilization);
        file.write_all(line.as_bytes()).unwrap();
    }
}

fn main() {
    let digits: u64 = 200000000;
    let processes = num_cpus::get();
    let cpu_model = if cfg!(target_os = "windows") {
        let output = Command::new("wmic")
            .args(&["cpu", "get", "name"])
            .output()
            .expect("Failed to execute command");
        let cpu_model = String::from_utf8_lossy(&output.stdout);
        let cpu_model = cpu_model.lines().nth(1).unwrap();
        cpu_model.trim().to_string()
    } else if cfg!(target_os = "linux") {
        let output = Command::new("cat")
            .args(&["/proc/cpuinfo"])
            .output()
            .expect("Failed to execute command");
        let cpu_model = String::from_utf8_lossy(&output.stdout);
        let cpu_model = cpu_model.lines().find(|line| line.starts_with("model name")).unwrap();
        let cpu_model = cpu_model.split(":").nth(1).unwrap();
        cpu_model.trim().to_string()
    } else if cfg!(target_os = "macos") {
        let output = Command::new("sysctl")
            .args(&["-n", "machdep.cpu.brand_string"])
            .output()
            .expect("Failed to execute command");
        let cpu_model = String::from_utf8_lossy(&output.stdout);
        cpu_model.trim().to_string()
    } else {
        String::new()
    };
    let os_info = if cfg!(target_os = "windows") {
        let output = Command::new("systeminfo")
            .args(&["|", "findstr", "/C:OS"])
            .output()
            .expect("Failed to execute command");
        let os_info = String::from_utf8_lossy(&output.stdout);
        let os_info = os_info.lines().nth(0).unwrap();
        let os_info = os_info.split(":").nth(1).unwrap();
        let os_info = os_info.trim().to_string();
        let os_info = os_info.split("Version:").nth(0).unwrap();
        os_info.trim().to_string()
    } else if cfg!(target_os = "linux") {
        let output = Command::new("lsb_release")
            .args(&["-d"])
            .output()
            .expect("Failed to execute command");
        let os_info = String::from_utf8_lossy(&output.stdout);
        let os_info = os_info.lines().nth(0).unwrap();
        let os_info = os_info.split(":").nth(1).unwrap();
        os_info.trim().to_string()
    } else if cfg!(target_os = "macos") {
        let output = Command::new("sw_vers")
            .args(&["-productName"])
            .output()
            .expect("Failed to execute command");
        let os_info = String::from_utf8_lossy(&output.stdout);
        os_info.trim().to_string()
    } else {
        String::new()
    };
    let execution_time_single_core = execution_time_for_multiprocessing(digits, 1);
    let execution_time_multi_core = execution_time_for_multiprocessing(digits, processes);
    println!("CPU model: {}", cpu_model);
    println!("Execution time for {} digits with single core is {}", digits, execution_time_single_core);
    println!("Execution time for {} digits with {} cores is {}", digits, processes, execution_time_multi_core);
    println!("Single core score for {} digits is {}", digits, calculate_multi_core_score(digits, execution_time_single_core));
    println!("Multi core score for {} digits is {}", digits, calculate_multi_core_score(digits, execution_time_multi_core));
    println!("Speedup for {} digits is {}", digits, execution_time_single_core / execution_time_multi_core);
    println!("Efficiency for {} digits is {}", digits, (execution_time_single_core / execution_time_multi_core) / processes as f64);
    println!("CPU utilization for {} digits is {}%", digits, 100.0 - (execution_time_multi_core / execution_time_single_core) * 100.0);
    let cpu_benchmark_doc = json!({
        "cpu_model": cpu_model,
        "execution_time_single_core": execution_time_single_core,
        "execution_time_multi_core": execution_time_multi_core,
        "single_core_score": calculate_multi_core_score(digits, execution_time_single_core),
        "multi_core_score": calculate_multi_core_score(digits, execution_time_multi_core),
        "speedup": execution_time_single_core / execution_time_multi_core,
        "efficiency": (execution_time_single_core / execution_time_multi_core) / processes as f64 * 100.0,
        "cpu_utilization": 100.0 - (execution_time_multi_core / execution_time_single_core) * 100.0,
        "os": os_info,
    });
    let uri = std::env::var("MONGODB_URI").unwrap();
    let client = Client::with_uri_str(&uri).unwrap();
    match client.database("taipan_benchmarks").collection("cpu_benchmarks").insert_one(cpu_benchmark_doc, None) {
        Ok(_) => println!("Inserted benchmark data into MongoDB"),
        Err(e) => println!("Failed to insert benchmark data into MongoDB: {}", e),
    }
}


