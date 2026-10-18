#include <evr/image_submission.hpp>
#include <iostream>
#include <stdexcept>
using evr::diagnostic::ImageSubmission;
int main() {
    unsigned checks=0;
    auto check=[&](bool condition,const char* name){if(!condition)throw std::runtime_error(name);++checks;std::cout<<"PASS "<<name<<'\n';};
    try {
        ImageSubmission image;
        check(!image.ready(),"new image has unknown layout");
        image.record(10,1,true);check(!image.ready(),"CPU recording is not submitted work");
        check(!image.submit(11,0,1,true)&&!image.ready(),"unrelated command cannot validate image");
        image.submit(10,2,2,true);check(image.ready()&&image.family==2&&image.layout==1,"actual submission assigns queue family");
        image.record(10,6,true);image.record(11,1,true);image.submit(11,0,3,true);
        check(!image.ready(),"pending command blocks even when latest recording submitted");
        image.submit(10,0,4,true);check(image.ready()&&image.layout==6,"submission order determines final layout");
        image.record(10,1,true);image.reset(10);check(!image.ready(),"unsubmitted command reset cannot reuse old evidence");
        image.record(10,1,true);image.submit(10,0,5,true);check(image.ready()&&image.submission==5,"fresh command recording recovers evidence");
        image.record(12,1,false);image.record(12,1,true);image.submit(12,0,6,true);
        check(!image.ready(),"partial range or ownership transfer stays unsupported");
        image={};image.record(1,1,true);image.submit(1,0,7,false);check(!image.ready(),"failed queue submit never validates image");
        image={};image.record(1,1,true);image.submit(1,0,8,true);image.reset(1);
        check(image.ready(),"reset after submission preserves actual image state");
        image={};for(uint64_t n=1;n<=129;++n)image.record(n,1,true);
        for(uint64_t n=1;n<=129;++n)image.submit(n,0,n,true);
        check(!image.ready(),"bounded metadata overflow fails closed");
        image={};for(uint64_t n=1;n<=128;++n)image.record(n,1,true);
        image.record(64,6,true);
        for(uint64_t n=128;n>0;--n)if(n!=64)image.submit(n,0,n,true);
        image.submit(64,0,129,true);
        check(image.ready()&&image.layout==6,"existing command can update at capacity and submit out of order");
        image={};image.record(1,1,true);image.submit(1,0,1,true);
        image.record(0,1,true);image.record(2,1,true);image.submit(2,0,2,true);
        check(!image.ready(),"invalid command permanently rejects stale and subsequent evidence");
        static_assert(noexcept(image.record(1,1,true))&&noexcept(image.submit(1,0,1,true))&&noexcept(image.reset(1)),
                      "image metadata updates must not allocate or throw");
        std::cout<<"PASS total "<<checks<<'\n';return 0;
    } catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
