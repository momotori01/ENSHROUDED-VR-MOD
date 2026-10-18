#include <evr/screen_keys.hpp>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace evr::screen;
int main(){try{unsigned n=0;auto ck=[&](bool b,int line=__builtin_LINE()){++n;if(!b)throw std::runtime_error(std::to_string(n)+" at line "+std::to_string(line));};
 using Events=std::vector<KeyEvent>;
 // The two hands together: the stick that is pushed further; a hand without a stick does not count.
 {const auto p=pad(.1f,.2f,true,-.9f,.1f,true,true,false);ck(p.x==-.9f&&p.y==.1f&&p.confirm&&!p.back);
  const auto q=pad(.5f,.5f,true,1,1,false,false,true);ck(q.x==.5f&&q.y==.5f&&q.back);
  const auto none=pad(1,1,false,1,1,false,false,false);ck(none.x==0&&none.y==0);}
 // A stick pushed down: the arrow once, up again 80 ms later, then again after 400 ms and every 150 ms.
 {Keys k;Pad down;down.y=-1;
  ck(k.update(down,1000)==Events{{Key::Down,true}}&&k.anyDown());
  ck(k.update(down,1050).empty());
  ck(k.update(down,1080)==Events{{Key::Down,false}}&&!k.anyDown());
  ck(k.update(down,1399).empty());
  ck(k.update(down,1400)==Events{{Key::Down,true}});
  ck(k.update(down,1480)==Events{{Key::Down,false}});
  ck(k.update(down,1549).empty()&&k.update(down,1550)==Events{{Key::Down,true}});
  // Let go: the pending release still comes, nothing after it.
  ck(k.update({},1560).empty()&&k.update({},1630)==Events{{Key::Down,false}}&&k.update({},3000).empty());}
 // Beyond 0.6 to press, below 0.4 to let go; the stronger axis alone.
 {Keys k;Pad p;p.x=.55f;ck(k.update(p,0).empty());
  p.x=.7f;p.y=.5f;ck(k.update(p,10)==Events{{Key::Right,true}});
  p.x=.45f;p.y=0;ck(k.update(p,100)==Events{{Key::Right,false}});   // still held (above 0.4): only the tap's release
  ck(k.update(p,410)==Events{{Key::Right,true}});                    // and it repeats
  p.x=.3f;ck(k.update(p,500)==Events{{Key::Right,false}});
  p.x=-.8f;ck(k.update(p,600)==Events{{Key::Left,true}});
  p.x=0;p.y=.9f;const auto e=k.update(p,610);ck(e==Events{{Key::Up,true}});   // left is let go, its release comes with its 80 ms
  ck(k.update(p,680)==Events{{Key::Left,false}});}
 // A / X and B / Y: down with the button, up with it.
 {Keys k;Pad p;p.confirm=true;ck(k.update(p,0)==Events{{Key::Confirm,true}}&&k.update(p,500).empty());
  p.confirm=false;p.back=true;const auto e=k.update(p,600);ck(e.size()==2&&e[0]==KeyEvent{Key::Confirm,false}&&e[1]==KeyEvent{Key::Back,true});
  // The plate goes while keys are down: every one of them is released.
  Pad both;both.back=true;both.y=1;k.update(both,700);const auto r=k.release();
  ck(r.size()==2&&r[0]==KeyEvent{Key::Up,false}&&r[1]==KeyEvent{Key::Back,false}&&!k.anyDown()&&k.release().empty());
  ck(k.update({},800).empty());}
 // The plate comes back ahead after the player has looked away from it for 1.5 s.
 {Recentre r;
  ck(!r.update(0,-2,0,-1,0)&&!r.update(0,-2,.5f,-.86f,5000));          // looking at it, or 30 degrees off
  ck(!r.update(0,-2,1,0,6000)&&!r.update(0,-2,1,0,7400)&&r.update(0,-2,1,0,7500));   // 90 degrees off, for 1.5 s
  ck(!r.update(0,-2,1,0,7600)&&!r.update(0,-2,0,-1,8000)&&!r.update(0,-2,0,1,8100)&&!r.update(0,-2,0,1,9500)&&r.update(0,-2,0,1,9600));
  ck(!r.update(0,-2,0,0,10000)&&!r.update(0,0,1,0,10000));}             // straight up or down, or standing in the plate
 {Level l;l.placed(-.1f);                                                 // placed looking nearly level
  ck(!l.update(.1f,-.1f,0)&&!l.update(-.2f,.3f,100));                     // within 25 cm, any look: stays
  ck(!l.update(.4f,0,1000)&&!l.update(.4f,0,1900)&&l.update(.4f,0,2000));  // 40 cm above for 1 s: level again
  ck(!l.update(-.3f,0,3000)&&!l.update(.1f,0,3500)&&!l.update(-.3f,0,3600)&&!l.update(-.3f,0,4500)&&l.update(-.3f,0,4600));  // a break starts it again
  ck(!l.update(.1f,0,6000)&&!l.update(.1f,0,9000));                       // placed level: looking level does nothing
  // the friend's run: placed 57 degrees down (headset on the desk), then worn - looking 20 degrees down, 8 cm higher
  l.placed(-.84f);ck(!l.update(.08f,-.81f,10000)&&!l.update(.08f,-.81f,12000));   // still steep: stays
  ck(!l.update(.08f,-.34f,13000)&&!l.update(.08f,-.34f,13400)&&l.update(.08f,-.34f,13500));   // level enough for 0.5 s
  l.placed(-.34f);ck(!l.update(.08f,-.34f,14000)&&!l.update(.08f,-.34f,16000));    // placed there: stays
  l.placed(.9f);ck(!l.update(0,.9f,17000)&&!l.update(0,.2f,17100)&&l.update(0,.2f,17600));   // placed looking up as well
  ck(!l.update(NAN,0,18000)&&!l.update(0,NAN,18000));}                     // no pose: nothing
 std::cout<<"screen_keys "<<n<<" PASS\n";return 0;}
 catch(const std::exception& e){std::cerr<<"screen_keys FAIL "<<e.what()<<"\n";return 1;}}
