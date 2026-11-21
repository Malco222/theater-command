// The game's own test barrage (FBattleEffects::RunTest / PlayTest), replayed on the kit with the game's camera on the
// machine gun, to see what room the gun finds when it fires and whether its brass is thrown and laid.
#include <cmath>
#define private public
#include "Effects/TheaterFxCore.h"
#undef private
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <string>
#include <vector>
using namespace TheaterFx;
struct Flat : Recipe {
 std::map<std::string,double> N; std::map<std::string,std::string> W;
 bool Find(const char* P, double& O) const override { auto I=N.find(P); if(I==N.end()) return false; O=I->second; return true; }
 bool FindWord(const char* P, std::string& O) const override { auto I=W.find(P); if(I==W.end()) return false; O=I->second; return true; }
};
double G(void*, double, double){ return 0; }
struct Shot { double When; int Kind; double X, Y; const char* Size; const char* Ground; };
int main(int, char** A){
 Flat B; std::ifstream F(A[1]); std::string L;
 while(std::getline(F,L)){ auto T=L.find('\t'); if(T==std::string::npos) continue; auto K=L.substr(0,T), V=L.substr(T+1); if(!V.empty()&&V[0]=='=') B.W[K]=V.substr(1); else B.N[K]=std::stod(V);}
 const double Fps = A[2] ? atof(A[2]) : 30.0;
 const double Turn = A[2] && A[3] ? atof(A[3]) : 0.0; // degrees round the gun
 Battle X; X.Setup(&B,&G,nullptr,3); X.SetWind(Vec(1.6,0.4,0));
 std::vector<Shot> S = {
  {0.5,0,-30,0,"light","earth"},{1.5,0,-18,4,"mortar","earth"},{2.7,0,-4,-2,"gun75","earth"},{4.0,0,12,3,"how105","earth"},{5.6,0,32,6,"how150","earth"},
  {7.5,0,-20,30,"how105","sand"},{8.6,0,0,34,"how105","snow"},{9.7,0,20,30,"how105","mud"},{11.0,0,40,90,"bomb250","earth"},
  {13.5,1,-8,14,"gun75","earth"},{14.2,2,6,12,"","earth"},{15.0,3,-40,-6,"tank_gun",""},{15.5,4,-36,-10,"mg",""},{16.0,5,24,18,"",""},{18.0,4,-36,-10,"mg",""},{19.0,6,0,60,"heavy",""} };
 X.OilFire(0.0, 9001, Vec(-250,900,0), 100.0, "sunk", 0.0, 90.0);
 X.OilFire(0.0, 9002, Vec(600,2500,0), 70.0, "burning", 0.0, 90.0);
 Battle::View V; V.PixelsPerRadian = 1072.0/(2.0*std::tan(21.0*3.14159265/180.0));
 const Vec Foot(-36,-10,0); const double Down=22.0*3.14159265/180.0, Tr=Turn*3.14159265/180.0;
 V.Eye = Foot + Vec(std::cos(Tr),std::sin(Tr),0)*(-18.75*std::cos(Down)) + Vec(0,0,18.75*std::sin(Down));
 V.Forward=(Foot-V.Eye).Unit(); V.Right=V.Forward.Cross(Vec(0,0,1)).Unit(); V.Up=V.Right.Cross(V.Forward).Unit();
 if(!getenv("NOCULL")){ V.HalfAcross=std::tan(21.0*3.14159265/180.0); }
 const double Cycle=24.0; int Frames=static_cast<int>(48.0*Fps);
 for(int f=0;f<Frames;f++){ double t=f/Fps;
  for(Shot& s: S){ if(s.When>t) continue; s.When+=Cycle; Vec W(s.X,s.Y,0);
   switch(s.Kind){
    case 0: X.Shell(t,W,s.Size,s.Ground,false); break;
    case 1: X.Shell(t,W+Vec(0,0,1.2),s.Size,s.Ground,true); break;
    case 2: X.Skip(t,W,Vec(1,0.3,0),s.Ground); break;
    case 3: { Vec Gun=W+Vec(0,0,2.0); X.Muzzle(t,Gun,(Vec(12,3,1)-Gun).Unit(),s.Size,""); break; }
    case 4: { size_t Before=X.Bits.size(); size_t Asked=X.RoomAsked(); size_t Seen=X.RoomSeen();
      X.Shots(t,13,W+Vec(0,0,0.6),Vec(20,20,0.5),s.Size,25,15.0,"earth");
      size_t Cases=0; for(size_t i=Before;i<X.Bits.size();i++) if(X.Bits[i].Form==Bit::Shape::Plate && X.Bits[i].bThrown) Cases++;
      printf("t %.2f MG fires: room asked %zu, seen %zu of %d (cases need under %d), in sight %zu, bits %zu -> +%zu bits, %zu cases thrown\n", t, Asked, Seen, X.MaxPuffs, X.MaxPuffs*3/5, X.InSight, Before, X.Bits.size()-Before, Cases); break; }
    case 5: X.Burn(t,W,3.0,12.0); break;
    case 6: X.AirFlak(t,W+Vec(0,0,60),s.Size); break; } }
  int n=X.Draw(t,V);
  if(f%static_cast<int>(Fps*2)==0){ size_t Lying=0; for(const Bit& b: X.Bits) if(b.Form==Bit::Shape::Plate && b.bThrown) Lying++;
   printf("t %5.1f drawn %4d in sight %4zu bits %5zu cases alive %zu\n", t, n, X.InSight, X.Bits.size(), Lying); }
 }
}
