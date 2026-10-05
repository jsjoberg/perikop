#include "ui/main_frame.hpp"
#include <wx/app.h>
#include <wx/stdpaths.h>
#include <wx/filename.h>
#include <wx/fontenum.h>
#include <wx/image.h>
#include <wx/timer.h>
#include <filesystem>
#include <iostream>
#ifdef __APPLE__
#include <CoreText/CoreText.h>
#endif
namespace {
std::filesystem::path path(const wxString& value) { auto bytes=value.ToUTF8(); return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(bytes.data()),bytes.length())); }
}
class ReaderApp final:public wxApp {
public:
    bool OnInit() override {
        SetAppName("orthodox-reader");SetVendorName("orthodox-reader");
        wxInitAllImageHandlers();
        bool smoke=false,reader=false;wxString resource_override,screenshot;
        auto date=ortho::local_civil_date();
        for(int i=1;i<argc;++i) {
            const wxString arg=argv[i];
            if(arg=="--smoke-test")smoke=true;
            else if(arg=="--reader")reader=true;
            else if((arg=="--resources"||arg=="--date"||arg=="--screenshot")&&i+1<argc) {
                const wxString value=argv[++i];
                if(arg=="--resources")resource_override=value;
                if(arg=="--screenshot")screenshot=value;
                if(arg=="--date") { auto parsed=ortho::parse_date(value.ToStdString());if(!parsed){std::cerr<<parsed.error()<<'\n';return false;}date=*parsed; }
            } else {std::cerr<<"Unknown or incomplete argument: "<<arg.ToStdString()<<'\n';return false;}
        }
        try {
            std::vector<std::filesystem::path> candidates;
            if(!resource_override.empty())candidates.push_back(path(resource_override));
            else {
                const auto executable=path(wxStandardPaths::Get().GetExecutablePath()).parent_path();
#ifdef __APPLE__
                candidates.push_back(executable.parent_path()/"Resources");
#endif
                candidates.push_back(executable/"resources");
                candidates.push_back(executable.parent_path()/"share/orthodox-reader/resources");
                candidates.emplace_back(ORTHO_RESOURCE_INSTALL_PATH);
            }
            std::filesystem::path resources;
            for(const auto& candidate:candidates)if(std::filesystem::exists(candidate/"corpus/corpus.db")){resources=candidate;break;}
            if(resources.empty())throw std::runtime_error("Bundled resources missing. Rebuild or use --resources PATH.");
            for(const auto* file:{"Literata-Regular.ttf","Literata-Italic.ttf","IBMPlexSans-Regular.ttf","IBMPlexSans-Medium.ttf"}) {
                const auto file_path=(resources/"fonts"/file).u8string();
                #ifdef __APPLE__
                const auto* bytes=reinterpret_cast<const UInt8*>(file_path.c_str());
                CFURLRef url=CFURLCreateFromFileSystemRepresentation(nullptr,bytes,file_path.size(),false);
                CFErrorRef error=nullptr;
                const bool loaded=url && CTFontManagerRegisterFontsForURL(url,kCTFontManagerScopeProcess,&error);
                if(url)CFRelease(url);
                if(error)CFRelease(error);
#else
                const bool loaded=wxFont::AddPrivateFont(wxString::FromUTF8(reinterpret_cast<const char*>(file_path.c_str())));
#endif
                if(!loaded)throw std::runtime_error(std::string("Cannot load bundled font: ")+file);
            }
            corpus_=std::make_unique<ortho::CorpusDb>(resources/"corpus/corpus.db");
            if(smoke) {
                test_path_=std::filesystem::temp_directory_path()/std::filesystem::path("orthodox-reader-smoke-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
                user_=std::make_unique<ortho::UserDb>(test_path_/"user.db");
            } else user_=std::make_unique<ortho::UserDb>(path(wxStandardPaths::Get().GetUserLocalDataDir())/"user.db");
            #if wxCHECK_VERSION(3,3,0)
            SetAppearance(static_cast<wxApp::Appearance>(user_->load().theme));
#endif
            auto* frame=new ortho::MainFrame(*corpus_,*user_,date);SetTopWindow(frame);frame->Show();
            if(reader)frame->open_psalm();
            if(smoke) {
                timer_=std::make_unique<wxTimer>(this);
                Bind(wxEVT_TIMER,[this,frame,screenshot](wxTimerEvent&){
                    try{smoke_exit_=frame->smoke_test(screenshot)?0:1;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';smoke_exit_=1;}
                    std::cout<<(smoke_exit_==0?"UI smoke passed: fonts, rendering, themes, parallel modes, stable date.\n":"UI smoke failed.\n");
                    frame->Close(true);
                });timer_->StartOnce(300);
            }
            return true;
        } catch(const std::exception& e) {
            if(smoke)std::cerr<<e.what()<<'\n';else wxMessageBox(wxString::FromUTF8(e.what()),"Ortodox läsare",wxOK|wxICON_ERROR);
            return false;
        }
    }
    int OnRun() override { const int status=wxApp::OnRun();return smoke_exit_>=0?smoke_exit_:status; }
    int OnExit() override {
        timer_.reset();user_.reset();corpus_.reset();
        if(!test_path_.empty()){std::error_code error;std::filesystem::remove_all(test_path_,error);}
        return wxApp::OnExit();
    }
private:
    std::unique_ptr<ortho::CorpusDb> corpus_;
    std::unique_ptr<ortho::UserDb> user_;
    std::unique_ptr<wxTimer> timer_;
    std::filesystem::path test_path_;
    int smoke_exit_=-1;
};
wxIMPLEMENT_APP(ReaderApp);
