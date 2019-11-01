//
// Created by root on 10/30/19.
//

#include <iostream>
#include <algorithm>
#include <chrono>
#include <vector>
#include <sys/wait.h>
#include "simdjson/jsonstream.h"
#include <map>

#include "simdjson/isadetection.h"
#include "simdjson/jsonioutil.h"
#include "simdjson/jsonparser.h"
#include "simdjson/parsedjson.h"

#define NB_ITERATION 5
#define MIN_BATCH_SIZE 150
#define MAX_BATCH_SIZE 5000000

bool test_baseline = false;
bool test_per_batch = true;
bool test_best_batch = true;

int main (int argc, char *argv[]){

    if (argc <= 1) {
        std::cerr << "Usage: " << argv[0] << " <jsonfile>" << std::endl;
        exit(1);
    }
    const char *filename = argv[1];
    simdjson::padded_string p;
    try {
        std::wclog << "loading " << filename << "\n" << std::endl;
        simdjson::get_corpus(filename).swap(p);
    } catch (const std::exception &) { // caught by reference to base
        std::cerr << "Could not load the file " << filename << std::endl;
        return EXIT_FAILURE;
    }
if (test_baseline) {
    std::wclog << "Baseline: Getline + normal parse... " << std::endl;
    std::cout << "Gigabytes/second\t" << "Nb of documents parsed" << std::endl;
    for (auto i = 0; i < 3; i++) {
        //Actual test
        simdjson::ParsedJson pj;
        bool allocok = pj.allocate_capacity(p.size());
        if (!allocok) {
            std::cerr << "failed to allocate memory" << std::endl;
            return EXIT_FAILURE;
        }
        std::istringstream ss(std::string(p.data(), p.size()));

        auto start = std::chrono::steady_clock::now();
        int count = 0;
        std::string line;
        int parse_res = simdjson::SUCCESS;
        while (getline(ss, line)) {
            parse_res = simdjson::json_parse(line, pj);
            count++;
        }

        auto end = std::chrono::steady_clock::now();

        std::chrono::duration<double> secs = end - start;
        double speedinGBs = (p.size()) / (secs.count() * 1000000000.0);
        std::cout << speedinGBs << "\t\t\t\t" << count << std::endl;

        if (parse_res != simdjson::SUCCESS) {
            std::cerr << "Parsing failed" << std::endl;
            exit(1);
        }
    }
}

std::map<size_t, double> batch_size_res;
if(test_per_batch) {
    std::wclog << "Jsonstream: Speed per batch_size... from " << MIN_BATCH_SIZE
               << " bytes to " << MAX_BATCH_SIZE << " bytes..." << std::endl;
    std::cout << "Batch Size\t" << "Gigabytes/second\t" << "Nb of documents parsed" << std::endl;
    for (size_t i = MIN_BATCH_SIZE; i <= MAX_BATCH_SIZE; i += (MAX_BATCH_SIZE - MIN_BATCH_SIZE)/ 10000) {
        batch_size_res.insert(std::pair<size_t, double>(i, 0));
        int count;
        for (size_t j = 0; j < 5; j++) {
            //Actual test
            simdjson::ParsedJson pj;
            simdjson::JsonStream js{p.data(), p.size(), i};
            int parse_res = simdjson::SUCCESS_AND_HAS_MORE;

            auto start = std::chrono::steady_clock::now();
            count = 0;
            while (parse_res == simdjson::SUCCESS_AND_HAS_MORE) {
                parse_res = js.json_parse(pj);
                count++;
            }

            auto end = std::chrono::steady_clock::now();

            std::chrono::duration<double> secs = end - start;
            double speedinGBs = (p.size()) / (secs.count() * 1000000000.0);
            if (speedinGBs > batch_size_res.at(i))
                batch_size_res[i] = speedinGBs;

            if (parse_res != simdjson::SUCCESS) {
                std::cerr << "Parsing failed" << std::endl;
                exit(1);
            }
        }
        std::cout << i << "\t\t" << batch_size_res.at(i) << "\t\t\t\t" << count << std::endl;

    }
}
if(test_best_batch) {
    size_t optimal_batch_size;
    if(test_per_batch)
        optimal_batch_size = (*max_element(batch_size_res.begin(), batch_size_res.end())).first;
    else
        optimal_batch_size = MIN_BATCH_SIZE;
    std::wclog << "Starting speed test... Best of " << NB_ITERATION << " iterations..." << std::endl;
    std::wclog << "Seemingly optimal batch_size: " << optimal_batch_size << "..." << std::endl;
    std::vector<double> res;
    for (int i = 0; i < NB_ITERATION; i++) {

        //Actual test
        simdjson::ParsedJson pj;
        simdjson::JsonStream js{p.data(), p.size(), 4000000};
        int parse_res = simdjson::SUCCESS_AND_HAS_MORE;

        auto start = std::chrono::steady_clock::now();

        while (parse_res == simdjson::SUCCESS_AND_HAS_MORE) {
            parse_res = js.json_parse(pj);
        }


        auto end = std::chrono::steady_clock::now();

        std::chrono::duration<double> secs = end - start;
        res.push_back(secs.count());

        if (parse_res != simdjson::SUCCESS) {
            std::cerr << "Parsing failed" << std::endl;
            exit(1);
        }

    }

    std::min(res.begin(), res.end());

    double min_result = *min_element(res.begin(), res.end());
    double speedinGBs = (p.size()) / (min_result * 1000000000.0);


    std::cout << "Min:  " << min_result << " bytes read: " << p.size()
              << " Gigabytes/second: " << speedinGBs << std::endl;
}



    return 0;
}