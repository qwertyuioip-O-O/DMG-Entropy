#include "rmc.hpp"
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <algorithm>

namespace {
int piece_type(char c) {
    switch(c) {case 'N':return 2;case 'B':return 3;case 'R':return 4;case 'Q':return 5;case 'K':return 6;default:return 1;}
}
rmc::Move san_move(const rmc::Position& p,std::string san) {
    while(!san.empty()&&(san.back()=='+'||san.back()=='#'||san.back()=='!'||san.back()=='?'))san.pop_back();
    std::replace(san.begin(),san.end(),'0','O');
    if(san=="O-O"||san=="O-O-O") {
        unsigned from=p.side?60:4;
        unsigned to=p.side?(san=="O-O"?62:58):(san=="O-O"?6:2);
        return rmc::Move{uint8_t(from),uint8_t(to),0};
    }
    unsigned promo=0;
    if(san.size()>=2&&san[san.size()-2]=='=') {
        switch(san.back()) {case 'Q':promo=1;break;case 'R':promo=2;break;case 'B':promo=3;break;case 'N':promo=4;break;default:throw std::runtime_error("invalid SAN promotion: "+san);}
        san.resize(san.size()-2);
    }
    if(san.size()<2)throw std::runtime_error("invalid SAN token");
    char f=san[san.size()-2],r=san.back();
    if(f<'a'||f>'h'||r<'1'||r>'8')throw std::runtime_error("invalid SAN destination: "+san);
    int destination=(r-'1')*8+(f-'a');
    san.resize(san.size()-2);
    int type=1;
    if(!san.empty()&&san.front()>='A'&&san.front()<='Z') {type=piece_type(san.front());san.erase(san.begin());}
    bool taking=san.find('x')!=std::string::npos;
    san.erase(std::remove(san.begin(),san.end(),'x'),san.end());
    std::vector<rmc::Move> matching;
    for(auto m:p.legal_moves()) {
        int actual=p.board[m.from];if(actual<0)actual=-actual;
        if(actual!=type||m.to!=destination||m.promotion!=promo)continue;
        bool capture=p.board[m.to]!=0 || (type==1 && (m.from&7)!=(m.to&7));
        if(capture!=taking)continue;
        if(san.size()>=1) {
            if(san[0]>='a'&&san[0]<='h' && ((m.from&7)!=san[0]-'a'))continue;
            if(san[0]>='1'&&san[0]<='8' && ((m.from>>3)!=san[0]-'1'))continue;
        }
        if(san.size()>=2) {
            if(san[1]>='a'&&san[1]<='h' && ((m.from&7)!=san[1]-'a'))continue;
            if(san[1]>='1'&&san[1]<='8' && ((m.from>>3)!=san[1]-'1'))continue;
        }
        matching.push_back(m);
    }
    if(matching.size()!=1)throw std::runtime_error("ambiguous/illegal SAN " +san+f+r+" matches="+std::to_string(matching.size()));
    return matching[0];
}
}
int main(int argc,char** argv) {
 try {
    if(argc!=3)throw std::runtime_error("usage: san_to_uci input.san output.uci");
    std::ifstream input(argv[1]);if(!input)throw std::runtime_error("cannot read input");
    std::ofstream output(argv[2]);if(!output)throw std::runtime_error("cannot write output");
    rmc::Position p=rmc::Position::standard();
    std::string line,word;
    unsigned count=0;
    while(std::getline(input,line)) {
        if(line=="---") {
            p=rmc::Position::standard();output<<"\n";continue;
        }
        std::istringstream ss(line);
        while(ss>>word) {
            if(word[0]=='['||word[0]=='%'||word[0]=='{'||word[0]=='(')continue;
            if(word=="1-0"||word=="0-1"||word=="1/2-1/2"||word=="*")continue;
            auto dot=word.rfind('.');
            if(dot!=std::string::npos) {
                if(dot+1==word.size())continue;
                word=word.substr(dot+1);
            }
            auto m=san_move(p,word);
            if(!p.apply_checked(m))throw std::runtime_error("could not apply " +word);
            output<<rmc::to_uci(m)<<' ';
            ++count;
        }
    }
    std::cout<<"converted "<<count<<" moves\n";
 } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
