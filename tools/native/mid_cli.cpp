#include "dmg_mid.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
namespace fs=std::filesystem;
static std::vector<uint8_t> read(const fs::path& path){
    std::ifstream file(path,std::ios::binary);
    if(!file)throw std::runtime_error("cannot read "+path.string());
    return {std::istreambuf_iterator<char>(file),{}};
}
static void write(const fs::path& path,const std::vector<uint8_t>& data){
    std::ofstream file(path,std::ios::binary);
    if(!file)throw std::runtime_error("cannot write "+path.string());
    file.write(reinterpret_cast<const char*>(data.data()),data.size());
    if(!file)throw std::runtime_error("failed to write "+path.string());
}
// No inference from existing archives: the decoder always uses the on-disk
// format marker and profile byte. Only *new* pack calls use the default.
int main(int argc,char** argv){
 try{
    if(argc==2 && std::string(argv[1])=="main"){
        std::cout<<"Main DMT profile: "<<unsigned(rmc::MID_DEFAULT_PROFILE)
                 <<" ("<<rmc::mid_name(rmc::MID_DEFAULT_PROFILE)<<")\n"
                 <<"Alternatives: 119, 182, 228, 231 (explicit pack profile).\n"
                 <<"Compression candidate: 250 (r12-joint-all).\n"
                 <<"Structure experiment: r13-no-specialists (251/mask95).\n";
    }else if(argc>=4&&std::string(argv[1])=="pack"){
        rmc::MidProfile chosen=rmc::MID_DEFAULT_PROFILE;
        bool explicit_profile=false;
        int structure_mask=-1;
        bool pawn_rook=false,strong_r16=false,source_r16=false;
        std::string selector=argv[2];
        if(selector=="best"||selector=="default"){
            explicit_profile=true;
        }else if(selector=="r13-no-specialists"){
            chosen=rmc::MidProfile::R13Structure;explicit_profile=true;structure_mask=95;
        }else if(selector=="r16-source-model"){
            chosen=rmc::MidProfile::R16SourceModel;explicit_profile=true;source_r16=true;
        }else if(selector=="r16-strong"){
            chosen=rmc::MidProfile::R16SourceModel;explicit_profile=true;strong_r16=true;
        }else if(selector=="r14-pawn-rook"){
            chosen=rmc::MidProfile::R14Structure;explicit_profile=true;pawn_rook=true;
        }else{
            for(unsigned i=0;i<rmc::MID_PROFILE_COUNT;++i){
                auto p=rmc::MidProfile(i);
                if(selector==rmc::mid_name(p)){
                    chosen=p;explicit_profile=true;break;
                }
            }
            // An omitted profile is only accepted for .dmt output filenames,
            // preventing a typo in a profile name from silently selecting 234.
            if(!explicit_profile && fs::path(selector).extension()!=".dmt")
                throw std::runtime_error("unknown DMT profile (or output must end in .dmt)");
        }
        const int archive_arg=explicit_profile?3:2;
        const int first_input=archive_arg+1;
        if(argc<=first_input)
            throw std::runtime_error("need at least one input .uci game");
        std::vector<std::vector<rmc::Move>> games;
        for(int i=first_input;i<argc;++i){auto bytes=read(argv[i]);
            games.push_back(rmc::parse_moves(std::string(bytes.begin(),bytes.end())));}
        auto archive=source_r16?rmc::mid_encode_source_model(games,{1,2,128}):strong_r16?rmc::mid_encode_source_model(games,{0,2,128}):pawn_rook?rmc::mid_encode_structural(games,{95,7,1}):structure_mask<0?rmc::mid_encode(games,chosen):
            rmc::mid_encode_structure(games,uint8_t(structure_mask));
        write(argv[archive_arg],archive.bytes);
        std::cout<<"profile "<<unsigned(chosen)<<" ("<<rmc::mid_name(chosen)
                 <<") / "<<games.size()<<" games / "<<archive.payload_bits
                 <<" bits / "<<archive.bytes.size()<<" bytes\n";
    }else if(argc==4&&std::string(argv[1])=="unpack"){
        auto games=rmc::mid_decode(read(argv[2]));fs::create_directories(argv[3]);
        for(size_t k=0;k<games.size();++k){
            std::ofstream output(fs::path(argv[3])/("game_"+std::to_string(k+1)+".uci"));
            output<<rmc::format_moves(games[k])<<"\n";
            if(!output)throw std::runtime_error("cannot write output");
        }
        std::cout<<"decoded "<<games.size()<<" games\n";
    }else throw std::runtime_error("usage: dmg_mid main | dmg_mid pack [PROFILE|best|default] file.dmt *.uci | dmg_mid unpack file.dmt output_dir");
 }catch(const std::exception&e){std::cerr<<"ERROR: "<<e.what()<<"\n";return 1;}
}
