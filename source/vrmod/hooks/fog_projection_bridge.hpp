// Fog 192e70 rebuilds projection at 192f68, independently of f7c50.
// Keep temporal reprojection in the same optical space as this private eye.
namespace fogProjection {
inline unsigned policy=0; // 0 off, 1 observe, 2 match, 3 bounded Mock comparison at frame 1300
inline bool failed=false;
inline std::array<uint64_t,2> converted{},readyChecks{},warmupCalls{};
struct Context{const void* camera;const game::ProjectionDescriptor* descriptor;unsigned eye,calls=0;uint64_t frame,serial;uint32_t thread;};
inline thread_local Context* current=nullptr;
struct Scope {
 Context context;Context* previous;
 Scope(const void* camera,const game::ProjectionDescriptor* descriptor,unsigned eye,uint64_t frame,uint64_t serial)
 :context{camera,descriptor,eye,0,frame,serial,GetCurrentThreadId()},previous(current){if(policy)current=&context;}
 ~Scope(){current=previous;}
};
inline void verify(){
 if(!policy)return;
 const auto* call=reinterpret_cast<const uint8_t*>(baseAddress+0x1bb3ad);int32_t delta=0;std::memcpy(&delta,call+1,4);   // 1.0: 1babc0 calls 1180f0 there (June 192f68 -> e4a50)
 if(*call!=0xe8||reinterpret_cast<uintptr_t>(call+5)+delta!=baseAddress+0x118270)
  throw std::runtime_error("fog projection callsite differs");
 // 1.0: the fog stage's camera is the render arguments +0x38 as before, now loaded from the frame (rbp+2b6c8) at 1386f7.
 const auto cameraLoad=parsePattern("48 8B 85 C8 B6 02 00 48 83 C0 38 48 89 85 68 16 00 00");
 if(scan(reinterpret_cast<const uint8_t*>(baseAddress+0x138897),cameraLoad.size(),cameraLoad)!=std::vector<size_t>{0})
  throw std::runtime_error("fog camera owner load differs");
 logFile->event("fog_projection_enabled","\"policy\":"+std::to_string(policy)+",\"private_eye_only\":true");
}
void convert(void* output,const void* camera,uintptr_t caller) noexcept {
 if(!policy||!current||caller!=baseAddress+0x1bb3b2)return;
 try{
  const ObserverLastError preserve;auto& c=*current;
  if(failed||c.eye>1||GetCurrentThreadId()!=c.thread||camera!=c.camera||!c.descriptor||eyeProjection!=c.descriptor||++c.calls!=1)
   throw std::runtime_error("fog projection private scope differs");
  const auto source=read<std::array<uint8_t,0x50>>(reinterpret_cast<uintptr_t>(camera));
  const auto native=read<game::ProjectionDescriptor>(reinterpret_cast<uintptr_t>(output));
  if(read<uint32_t>(reinterpret_cast<uintptr_t>(native.bytes.data()))!=0||read<uint32_t>(reinterpret_cast<uintptr_t>(c.descriptor->bytes.data()))!=3)
   throw std::runtime_error("fog perspective descriptor type differs");
  const bool apply=policy==2||(policy==3&&c.frame>=1300);
  if(apply)std::memcpy(output,c.descriptor->bytes.data(),80);
  if(read<std::array<uint8_t,0x50>>(reinterpret_cast<uintptr_t>(camera))!=source)
   throw std::runtime_error("fog source camera changed");
  const bool matched=read<game::ProjectionDescriptor>(reinterpret_cast<uintptr_t>(output)).bytes==c.descriptor->bytes;
  if(apply&&!matched)throw std::runtime_error("fog projection output differs from eye");
  const auto count=++converted[c.eye];
  if(count<=3||count%120==0){
   const auto values=read<std::array<float,4>>(reinterpret_cast<uintptr_t>(native.bytes.data())+4);
   std::ostringstream f;f<<std::setprecision(10)<<"\"eye\":"<<c.eye<<",\"frame\":"<<c.frame<<",\"serial\":"<<c.serial<<",\"policy\":"<<policy
    <<",\"native_aspect_near_far_fov\":["<<values[0]<<','<<values[1]<<','<<values[2]<<','<<values[3]
    <<"],\"source_unchanged\":true,\"same_private_camera\":true,\"output_matches_eye\":"<<(matched?"true":"false")<<",\"scope\":\"192f6d_private_eye\"";
   logFile->event("fog_projection_sample",f.str());
  }
 }catch(const std::exception& e){failed=true;try{logFile->event("fog_projection_failure","\"reason\":"+jsonString(e.what()));}catch(...){}}
}
inline void audit(unsigned eye,bool before,bool after,uint64_t countBefore){
 if(!policy)return;
 const auto calls=converted.at(eye)-countBefore;
 if(before&&after){if(calls!=1)throw std::runtime_error("ready eye omitted fog projection");++readyChecks[eye];}
 else warmupCalls[eye]+=calls;
}
inline void retired(){
 if(!policy)return;
 logFile->event("fog_projection_retired","\"policy\":"+std::to_string(policy)+",\"left\":"+std::to_string(converted[0])+",\"right\":"+std::to_string(converted[1])+
  ",\"ready_checks\":["+std::to_string(readyChecks[0])+","+std::to_string(readyChecks[1])+"],\"warmup_calls\":["+
  std::to_string(warmupCalls[0])+","+std::to_string(warmupCalls[1])+"],\"failed\":"+(failed?"true":"false")+",\"hook_disabled\":true");
}
}
