#include "../../code/main/diagnostics/persistentlog.h"
#include <thread>
#include <vector>
#include <cassert>
#include <fstream>
#include <string>
#include <set>
#include <stdlib.h>

int main() {
    char directory[]="/tmp/shar-log-test-XXXXXX";
    assert(mkdtemp(directory));
    std::string active=std::string(directory)+"/srr2_health.log";
    std::string previous=std::string(directory)+"/srr2_health.previous.log";
    {
        PersistentLog log; assert(log.Open(directory,1024*1024));
        std::vector<std::thread> threads;
        for(int t=0;t<4;++t) threads.emplace_back([&log,t]{
            for(int i=0;i<250;++i) { char message[64]; snprintf(message,sizeof(message),"thread=%d record=%d",t,i); assert(log.Write("health",message)); }
        });
        for(auto& thread:threads) thread.join();
        log.Flush();
        std::ifstream input(active); std::string line; std::set<unsigned long long> sequences;
        while(std::getline(input,line)) {
            unsigned long long sequence=0; int thread=-1, record=-1;
            assert(sscanf(line.c_str(),"seq=%llu event=health thread=%d record=%d",&sequence,&thread,&record)==3);
            assert(thread>=0 && thread<4 && record>=0 && record<250);
            assert(sequences.insert(sequence).second);
        }
        assert(sequences.size()==1000);
        assert(log.Write("health", "first\nsecond\rthird"));
        assert(log.Write("health", std::string(5000, 'x').c_str()));
        log.Flush();
        std::ifstream edge(active); unsigned lines=0;
        while(std::getline(edge,line)) { ++lines; assert(line.size()<2048); }
        assert(lines==1002);
        assert(log.Open(directory,4096)); // Previous session survives relaunch.
        std::ifstream old(previous); assert(old.good());
        for(int i=0;i<300;++i) assert(log.Write("ui-draw","screen=Mission name=Cancel alpha=255 depth=0"));
        log.Flush();
        struct stat st; assert(stat(active.c_str(),&st)==0 && st.st_size<=4096);
        assert(stat(previous.c_str(),&st)==0 && st.st_size<=4096);
    }
    unlink(active.c_str()); unlink(previous.c_str()); rmdir(directory);
    puts("PASS: 1000 concurrent records, unique sequence, multiline sanitizing, truncation, relaunch retention, bounded rotation");
}
