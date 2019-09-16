//
// Created by exa on 9/7/19.
//
#include <chrono>
#include <zconf.h>
#include "pattern2.h"


std::string rule = "b3s23";
std::string glider = "3o$o$bo!";
std::string transforms[4] = {"identity", "rot90", "rot180", "rot270"};
apg::pattern* glidertable[1600];
apg::lifetree_abstract<uint32_t>* lt = new apg::lifetree<uint32_t, 1>(1000000000);

struct testresult {
    char apgcode[256] = {0};
    apg::pattern* col = nullptr;
    bool success = false;
};

void printpat(apg::pattern* pat){
    pat->write_rle(std::cout);
    std::cout << "pop:" << pat->popcount(1073750017) << std::endl << std::endl;
}

void genglidertable() {
    int i = 0;
    for(int x = -5; x < 5; x++) {
        for(int y = -5; y < 5; y++) {
            for (auto &r : transforms) {
                for(uint64_t t = 0; t < 4; t++) {
                    apg::pattern* pat = new apg::pattern(lt, glider, rule);
                    apg::pattern tmp = apg::pattern(*pat);
                    tmp = pat->advance2(t, 0);
                    tmp = tmp.shift(10, 10);
                    tmp = tmp.transform(r, 0, 0);
                    tmp = tmp.shift(x, y);
                    *pat = tmp;
                    glidertable[i] = pat;
                    i++;
                }
            }
        }
    }
}


bool testsane(std::string collision, int targetpop) {
    apg::pattern pat = apg::pattern(lt, collision, rule);
    for(int t = 0; t < 5; t++) {
        int pop = pat.popcount(1073750017);
        if(pop != targetpop){
            return false;
        }
        //printpat(&pat);
        pat = pat.advance2(1);
    }
    return true;
}


apg::pattern* gencol(apg::pattern* col, apg::pattern* sl, int ngliders) {
   *col = apg::pattern(*sl);
    apg::pattern tmp = *col;
    for(int i = 0; i < ngliders; i++) {
        int index = rand() % 1600;
        tmp |= *glidertable[index];
    }
    *col = tmp;
    return col;
}



void GetApgcodeOfPattern(apg::pattern* pat, char* buffer, uint buflen) {
    std::string s = pat->apgcode();

    if (s.length() >= buflen) {
        std::ostringstream ss;
        ss << "!" << (((s.length() >> 8) + 1) << 8);
        s = ss.str();
    }

    s.copy(buffer, buflen);
}


void testcol(apg::pattern* sl, int ngliders, testresult* res) {
    res->success = false;
    apg::pattern* col = gencol(res->col, sl, ngliders);
    apg::pattern advanced = col->advance2(1, 7);
    int pop = advanced.popcount(1073750017);
    if (pop == 0 || pop > 128) {
        return;
    }
    apg::pattern next = apg::pattern(advanced);
    next = next.advance2(1, 0);
    if(next != advanced) {
        return;
    }
    GetApgcodeOfPattern(&advanced, res->apgcode, 256);
    res->success = true;
}

std::string runtest(std::string apgcode, int ngliders) {
    apg::pattern pat = apg::pattern(lt, std::move(apgcode), rule);
    testresult res;
    res.col = new apg::pattern(lt, rule);
    std::string result;
    std::stringstream buffer;
    testcol(&pat, ngliders, &res);
    if(res.success) {
        buffer << res.apgcode;
        buffer << " ";
        res.col->write_rle(buffer);
        result = buffer.str();
    }
    delete res.col;
    return result;
}

std::string runsingle(char* apgcode, int ngliders) {
    std::string result;
    while (result.length() == 0) {
        result = runtest(apgcode, ngliders);
    }
    return result;
}

void loadcosts(const std::string &file, std::unordered_map<std::string, int>* costs, std::vector<std::string>* targets) {
    std::ifstream infile(file);
    std::string apgcode;
    int cost;
    std::string line;
    while (std::getline(infile, line))
    {
        std::istringstream iss(line);
        iss >> apgcode;
        iss >> cost;
        costs->insert(std::make_pair(apgcode, cost));
        targets->push_back(apgcode);
    }
}

int main(){
    std::unordered_map<std::string, int> costs;
    std::vector<std::string> targets;
    genglidertable();
    loadcosts("/home/exa/Dropbox/Programming/C Code/CLion/GameOfLife/costs.txt", &costs, &targets);
    ulong i = 0;
    ulong ntarget = targets.size();
    int maxcols = 1000;
    int ngliders = 2;
    srand(static_cast<unsigned int>(getpid()));
    int offset = rand();
    auto start = std::chrono::steady_clock::now();
    while(i < ULONG_MAX) { // basically an infinite loop, but stops my IDE from complaining
        lt->force_gc();
        std::string target = targets.at((i++ + offset) % ntarget);
        int cost = costs.at(target);
        if (cost == 9999) {
            continue;
        }
        apg::pattern targetpattern = apg::pattern(lt, target, rule);
        int targetpop = targetpattern.popcount(1073750017) + 5 * ngliders;
        for (int c = 0; c < maxcols; c++) {
            std::string res = runtest(target, ngliders);

            if(res.length() > 0) {
                std::istringstream iss(res);
                std::string output;
                std::string collision;
                iss >> output;
                unsigned long headerlen = 0;
                std::getline(iss, collision);
                headerlen += collision.length();
                std::getline(iss, collision);
                headerlen += collision.length();
                collision = res.substr(headerlen + output.length() + 2);
                if(costs.find(output) != costs.end()) {
                    int outcost = costs.at(output);
                    if (outcost > cost + ngliders) {

                        costs[output] = cost + ngliders;
                        if(testsane(collision, targetpop)) {
//                            std::cout << "reduced " << output << " from " << outcost
//                                      << " to " << costs.at(output) << " with" << std::endl << collision << std::endl;
                            std::cout << output << " " << outcost << " " << costs.at(output) << " " << std::endl << collision << " " << i << std::endl << std::endl;
                        }
                    }
                }
            }
        }
        if(i%100 == 0) {
            std::cout << i << std::endl << std::endl;
        }
    }
    return 0;
}

