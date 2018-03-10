#ifndef __AL_API_IMPL_H__
#define __AL_API_IMPL_H__

#include <regex>

template<typename T>
class al_api_proc{
    T m_cb;
protected:
    std::vector<std::string> m_prs;
public:
    al_api_proc(){};
    virtual void load(std::shared_ptr<restbed::Resource>& r, std::vector<std::string> prs)=0;    

    void api_proc(const std::shared_ptr< restbed::Session > session){
        std::string parament;
        int length;
        const auto request = session->get_request( );
        parament=get_parament(session);
        length = request->get_header( "Content-Length", 0 );
        session->fetch( length, [ request, parament, length, this]( const std::shared_ptr< restbed::Session > session, const restbed::Bytes& body){
                int rc=0;
                std::string body_str=std::string(body.begin(), body.end());
                std::string retmsg;
                m_cb(parament, body_str, retmsg);
                session->close( restbed::OK, retmsg, { { "Content-Length", std::to_string( retmsg.length() ) } } );
            });
    }

    std::string get_parament(const std::shared_ptr<restbed::Session> &s){
        const auto request = s->get_request( );
        std::string rcs="{";
        for(auto os=m_prs.begin(); os != m_prs.end(); os++) {
            if( rcs.length() != 1 ){
                rcs+=",";
            }
            std::string str=request->get_path_parameter( *os );
            if( str.length() != 0 ){
                rcs+=std::string("\"")+*os+std::string("\":\"")+str+std::string("\"");
            }
        }
        rcs+="}";
        return rcs;
    }
};

template<typename T>
class al_api_get:public al_api_proc<T>{
    typedef al_api_proc<T> Parent;
public:
    al_api_get():al_api_proc<T>(){};
    void load(std::shared_ptr<restbed::Resource>& r, std::vector<std::string> prs){
        r->set_method_handler( "GET", std::bind(&al_api_proc<T>::api_proc, this,std::placeholders::_1));
        Parent::m_prs=prs;
    }
};

template<typename T>
class al_api_post:public al_api_proc<T>{
    typedef al_api_proc<T> Parent;
public:
    al_api_post():al_api_proc<T>(){};
    void load(std::shared_ptr<restbed::Resource>& r, std::vector<std::string> prs){
        r->set_method_handler( "POST", std::bind(&al_api_proc<T>::api_proc,this,std::placeholders::_1));
        Parent::m_prs=prs;
    }    
};

template<typename T>
class al_api_put:public al_api_proc<T>{
    typedef al_api_proc<T> Parent;
public:
    al_api_put():al_api_proc<T>(){};
    void load(std::shared_ptr<restbed::Resource>& r, std::vector<std::string> prs){
        r->set_method_handler( "PUT", std::bind(&al_api_proc<T>::api_proc, this,std::placeholders::_1));
        Parent::m_prs=prs;
    }
};

template<typename T>
class al_api_delete:public al_api_proc<T>{
    typedef al_api_proc<T> Parent;
public:
    al_api_delete():al_api_proc<T>(){};
    void load(std::shared_ptr<restbed::Resource>& r, std::vector<std::string> prs){
        r->set_method_handler( "DELETE", std::bind(&al_api_proc<T>::api_proc,this,std::placeholders::_1));
        Parent::m_prs=prs;
    }
};

class al_api{
    std::shared_ptr<restbed::Resource> m_res;
    std::vector<std::string>  m_prs;
private:

    void parse_prs(std::string& s){

        std::regex self_regex("REGULAR EXPRESSIONS", std::regex_constants::ECMAScript);
        if (std::regex_search(s, self_regex)) {
            throw "Invalid url";
        }
 
        std::regex word_regex("[{](\\w+)[:]");
        auto words_begin = std::sregex_iterator(s.begin(), s.end(), word_regex);
        auto words_end = std::sregex_iterator();
 
        for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
            std::smatch match = *i;
            std::string match_str = match.str();
            match_str=match_str.substr(1, match_str.length()-2);
            m_prs.push_back(match_str);
        }
    }

public:
    al_api(std::string url){
        m_res=std::make_shared<restbed::Resource>();
        m_res->set_path( url );
        parse_prs(url);
        std::cout<<"size:"<<m_prs.size()<<std::endl;
        for(std::vector<std::string>::iterator ite=m_prs.begin(); ite != m_prs.end(); ite++){
            std::cout<<*ite<<std::endl;
        }
    }

    template< typename T>
    void setup(std::shared_ptr< restbed::Service >& s, al_api_proc<T>* t1){
        t1->load(m_res, m_prs);
        s->publish(m_res);
    }

    template< typename T, typename T1>
    void setup(std::shared_ptr< restbed::Service >& s, al_api_proc<T>* t1, al_api_proc<T1>* t2){
        t1->load(m_res, m_prs);        
        t2->load(m_res, m_prs);
        s->publish(m_res);        
    }

    template< typename T, typename T1, typename T2>
    void setup(std::shared_ptr< restbed::Service >& s,al_api_proc<T>* t1, al_api_proc<T1>* t2, al_api_proc<T2>* t3){
        t1->load(m_res, m_prs);        
        t2->load(m_res, m_prs);
        t3->load(m_res, m_prs);
        s->publish(m_res);
    }

    template< typename T, typename T1, typename T2, typename T3>
    void setup(std::shared_ptr< restbed::Service >& s,al_api_proc<T>* t1, al_api_proc<T1>* t2, al_api_proc<T2>* t3, al_api_proc<T3>* t4){
        t1->load(m_res, m_prs);        
        t2->load(m_res, m_prs);
        t3->load(m_res, m_prs);
        t4->load(m_res, m_prs);
        s->publish(m_res);
    }
    
};

#endif /*__AL_API_IMPL_H__*/

