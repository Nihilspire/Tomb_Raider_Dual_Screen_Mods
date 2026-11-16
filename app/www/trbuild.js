/* First-boot builder: makes everything the app shows from the player's own Tomb Raider (USA) disc, so none of the game's data
   ships with the app. Runs in the app page (Electron) and under Node for the tests.
   Input: readAt(offset, length) -> Promise<Uint8Array> for the disc image (Track 01 .bin, raw 2352-byte sectors, or an .iso),
   its size, put(path, bytes|string) -> Promise, and enc(w, h, rgba) -> Promise<bytes> that turns a picture into an image file.
   Output (paths under the app's gen folder):
     img/title.png, img/area_<name>.png   the title screen and the loading screens (DELDATA/*.RAW, RNC packed)
     names.json                           level names and key item names, read from TOMBRAID.EXE
     lv/<n>.json                          per level: rooms (box, heights, portals), items, secrets, the exit
     lv/<n>/<room>.<ext>                  the top-down room pictures (trmap.js)
     icons/<type>.<ext>                   the pickup sprites, for the map markers
     manifest.json                        {v, t, disc, ext, levels} */
(function(G){
'use strict';
var TRL=G.TRL||require('./trlevel.js'),TRM=G.TRM||require('./trmap.js'),RNC=G.RNC||require('./rnc.js');
var TB={VERSION:6};   /* bump when the built files change: the app then builds again on its own */
var FILES=['GYM','LEVEL1','LEVEL2','LEVEL3A','LEVEL3B','LEVEL4','LEVEL5','LEVEL6','LEVEL7A','LEVEL7B','LEVEL8A','LEVEL8B','LEVEL8C','LEVEL10A','LEVEL10B','LEVEL10C'];
TB.FILES=FILES;
/* loading screens: which file, and the levels it belongs to */
TB.AREAS={home:'GYMLOAD',peru:'AZTECLOA',greece:'GREEKLOA',egypt:'EGYPTLOA',atlantis:'ATLANLOA'};
TB.areaOf=function(n){return n===0?'home':n<=4?'peru':n<=9?'greece':n<=12?'egypt':'atlantis'};
function str(b,p,n){var s='';for(var i=0;i<n;i++)s+=String.fromCharCode(b[p+i]);return s}
function u32(b,p){return (b[p]|b[p+1]<<8|b[p+2]<<16|b[p+3]<<24)>>>0}

/* ---------------- disc ---------------- */
TB.open=async function(reader,size){var d={};
 var head=await reader(0,16);
 if(str(head,0,8)==='MComprHD'){   /* a .chd: sectors through chd.js (raw 2352-byte sectors, 24 bytes of header) */
  var C=G.CHD||require('./chd.js'),c=await C.open(reader);
  d.sec=async function(n,cnt){cnt=cnt||1;var out=new Uint8Array(cnt*2048);for(var k=0;k<cnt;k++)out.set((await c.sector(n+k)).subarray(24,2072),k*2048);return out}}
 else{
 if(str(head,0,4)==='FILE'||size<1000000)throw new Error('That looks like a .cue file. Pick the "(Track 01).bin" file next to it.');
 var sync=head[0]===0&&head[11]===0;for(var i=1;i<11;i++)if(head[i]!==0xFF)sync=false;
 var raw=sync&&size%2352===0;if(!raw&&size%2048!==0)throw new Error('This file is not a disc image (.bin, .iso or .chd).');
 d.sec=async function(n,cnt){cnt=cnt||1;if(!raw)return reader(n*2048,cnt*2048);
  var all=await reader(n*2352,cnt*2352),out=new Uint8Array(cnt*2048);for(var k=0;k<cnt;k++)out.set(all.subarray(k*2352+24,k*2352+2072),k*2048);return out};}
 var pvd=await d.sec(16);if(str(pvd,1,5)!=='CD001')throw new Error('This file is not a PlayStation disc image.');
 d.files={};
 async function walk(lba,sz,pre){var b=await d.sec(lba,Math.ceil(sz/2048));
  for(var p=0;p<b.length;){var L=b[p];if(!L){p=(Math.floor(p/2048)+1)*2048;continue}
   var el=u32(b,p+2),fs=u32(b,p+10),fl=b[p+25],nm=str(b,p+33,b[p+32]);
   if(nm!=='\0'&&nm!=='\x01'){var full=pre+nm.replace(/;1$/,'');if(fl&2){if(!pre)await walk(el,fs,full+'/')}else d.files[full]=[el,fs]}
   p+=L}}
 await walk(u32(pvd,158),u32(pvd,166),'');
 d.file=async function(name){var e=d.files[name];if(!e)return null;return (await d.sec(e[0],Math.ceil(e[1]/2048))).subarray(0,e[1])};
 if(G.TB_NOCHECK)return d;   /* tools/disc (ls.js, exediff.js) */
 /* the game itself: TOMBRAID.EXE on the later US discs (SLUS_001.52 there is the Greatest Hits demo menu that starts it);
    on the first US release (v1.0) SLUS_001.52 is the game */
 if(!d.files['PSXDATA/LEVEL1.PSX'])throw new Error('This is not the Tomb Raider (PlayStation) disc.');
 if(!d.files['SLUS_001.52'])throw new Error('This is not the US Tomb Raider disc (SLUS-00152). The app reads the US version.');
 d.exe=d.files['TOMBRAID.EXE']?'TOMBRAID.EXE':'SLUS_001.52';
 return d};

/* ---------------- names from the game (TOMBRAID.EXE, US) ---------------- */
/* level names: a table of 22 string pointers; key item names: tables of 16 (one per level) for puzzle 1-4, pickup 1-2 and key 1-4;
   other pickups: the inventory items (pointer to the name, then the inventory object number). Addresses are for SLUS-00152;
   every pointer is checked, and a missing name falls back to a plain one. */
TB.names=function(exe){var load=u32(exe,0x18),out={levels:[],items:{}};
 function s(ptr){var f=ptr-load+0x800;if(f<0x800||f>=exe.length)return '';var r='';while(exe[f]&&r.length<40)r+=String.fromCharCode(exe[f++]);return /^[ -~]*$/.test(r)?r:''}
 /* the tables sit together and move together between releases (v1.0 has them 0xE8 earlier than Rev 6): find the level table
    near where Rev 6 has it, by its first two names, and move the others by as much */
 var sh=null;for(var dd=0;dd<=0x2000&&sh==null;dd+=4)[dd,-dd].forEach(function(x){if(sh==null&&s(u32(exe,0x77f10+x))==="Lara's Home"&&s(u32(exe,0x77f14+x))==='Caves')sh=x});
 if(sh==null)throw new Error('Unexpected game file on the disc (the level names were not found).');
 for(var i=0;i<22;i++)out.levels.push(s(u32(exe,0x77f10+sh+4*i)));
 var inv={};for(var a=0x787ac+sh;a<0x78e2c+sh;a+=0x40){var nm=s(u32(exe,a)),ob=exe[a+4]|exe[a+5]<<8;if(nm)inv[ob]=nm}
 var T=[[110,0x780e8],[111,0x78188],[112,0x781f4],[113,0x7825c],[129,0x78364],[130,0x783f8],[131,0x7845c],[132,0x784c4]];
 for(var lv=0;lv<16;lv++){var m={};
  for(var t=84;t<=94;t++)if(inv[t+15])m[t]=inv[t+15];
  T.forEach(function(e){m[e[0]]=s(u32(exe,e[1]+sh+4*lv))||(e[0]<129?(inv[e[0]+4]||'Puzzle'):'Key')});
  m[126]=inv[127]||'Lead Bar';m[143]=m[144]=m[145]=inv[150]||'Scion';
  out.items[lv]=m}
 return out};

/* ---------------- one level ---------------- */
TB.KEY=[84,85,86,87,110,111,112,113,126,129,130,131,132,143,144,145];
TB.PERLEVEL=[110,111,112,113,126,129,130,131,132,143,144,145];
TB.level=function(L){var o={rooms:[],items:[],secrets:[],exit:[]};
 L.rooms.forEach(function(m,ri){
  o.rooms.push({x:m.x,z:m.z,xs:m.xs,zs:m.zs,yt:m.yTop,yb:m.yBottom,f:m.flags,alt:m.alt,
   /* openings: [room, x0, y0, z0, x1, y1, z1, normal y] as a world-space box (a line for wall openings, a rectangle for floor
      and ceiling ones) */
   p:m.portals.map(function(p){var b=[1e9,1e9,1e9,-1e9,-1e9,-1e9];p.v.forEach(function(v){for(var k=0;k<3;k++){b[k]=Math.min(b[k],v[k]);b[k+3]=Math.max(b[k+3],v[k])}});
    return [p.room,m.x+b[0],b[1],m.z+b[2],m.x+b[3],b[4],m.z+b[5],p.n[1]]})});
  m.sec.forEach(function(s,si){var inf=TRL.sectorInfo(L,s);if(!inf.trig)return;
   var x=m.x+Math.floor(si/m.zs)*1024+512,z=m.z+(si%m.zs)*1024+512,y=s.floor*256;
   inf.trig.acts.forEach(function(a){if(a[0]===10)o.secrets.push([a[1],ri,x,y,z]);if(a[0]===7)o.exit.push([ri,x,y,z])})})});
 /* the walkable boxes (the ones the game's enemies find their way over): [x0, z0, x1, z1, floor y], the boxes each one touches,
    and per room the box under each sector (column-major like the sectors, -1 for none), for the map's routes */
 var nb=L.boxes.length;o.boxes=L.boxes.map(function(b){return [b.x0,b.z0,b.x1,b.z1,b.floor]});
 o.ovl=L.boxes.map(function(b){var a=[],p=b.ov&0x3FFF;if(p>=L.overlaps.length)return a;for(var g=0;g<64&&p<L.overlaps.length;g++){var w=L.overlaps[p++],k=w&0x7FFF;if(k<nb)a.push(k);if(w&0x8000)break}return a});
 L.rooms.forEach(function(m,ri){o.rooms[ri].sb=m.sec.map(function(s){return s.box<nb?s.box:-1})});
 var spr={};L.sseq.forEach(function(q){spr[q.type]=1});
 /* [type, room, x, y, z, has a sprite, the file's flags: 0x100 hidden until a trigger shows it] */
 L.items.forEach(function(it,i){o.items.push([it.type,it.room,it.x,it.y,it.z,spr[it.type]?1:0,it.flags])});
 return o};
/* the first sprite of a sprite sequence as RGBA (pickup icons) */
TB.icon=function(L,type){var q=null;L.sseq.forEach(function(s){if(s.type===type)q=s});if(!q)return null;var t=L.stex[q.start];if(!t)return null;
 var w=t.u1-t.u0+1,h=t.v1-t.v0+1;if(w<2||h<2||w>256||h>256)return null;var px=new Uint8ClampedArray(w*h*4);
 for(var y=0;y<h;y++)for(var x=0;x<w;x++){var c=TRL.texel(L,t.tile,t.clut,t.u0+x,t.v0+y),o=(y*w+x)*4;if(!c)continue;px[o]=(c&31)*8.23;px[o+1]=((c>>5)&31)*8.23;px[o+2]=((c>>10)&31)*8.23;px[o+3]=255}
 return {w:w,h:h,rgba:px}};
/* the level's most used floor texture as RGBA, for the map's backdrop: object textures counted by the area of the up-facing
   faces that use them; see-through texels are filled with the texture's average colour */
TB.floorTile=function(L){var use={};
 L.rooms.forEach(function(m){var V=m.V;function f(ix,tex){var a=ix[0],b=ix[1],c=ix[2];
   var ux=V[3*b]-V[3*a],uy=V[3*b+1]-V[3*a+1],uz=V[3*b+2]-V[3*a+2],vx=V[3*c]-V[3*a],vy=V[3*c+1]-V[3*a+1],vz=V[3*c+2]-V[3*a+2];
   var nx=uy*vz-uz*vy,ny=uz*vx-ux*vz,nz=ux*vy-uy*vx,nl=Math.sqrt(nx*nx+ny*ny+nz*nz);if(!nl||Math.abs(ny)/nl<0.8)return;
   var t=tex&0x7FFF;use[t]=(use[t]||0)+nl*(ix.length===4?1:0.5)}
  m.quads.forEach(function(q){f([q[0],q[1],q[2],q[3]],q[4])});m.tris.forEach(function(t){f([t[0],t[1],t[2]],t[3])})});
 var best=-1,bw=0;for(var k in use)if(use[k]>bw){bw=use[k];best=+k}var ot=L.otex[best];if(!ot)return null;
 var u=ot.uv,u0=Math.min(u[0],u[2],u[4],u[6]),u1=Math.max(u[0],u[2],u[4],u[6]),v0=Math.min(u[1],u[3],u[5],u[7]),v1=Math.max(u[1],u[3],u[5],u[7]);
 var w=u1-u0+1,h=v1-v0+1;if(w<4||h<4)return null;var px=new Uint8ClampedArray(w*h*4),sum=[0,0,0],cnt=0;
 for(var y=0;y<h;y++)for(var x=0;x<w;x++){var c16=TRL.texel(L,ot.tile,ot.clut,u0+x,v0+y),o=(y*w+x)*4;if(!c16)continue;
  px[o]=(c16&31)*8.23;px[o+1]=((c16>>5)&31)*8.23;px[o+2]=((c16>>10)&31)*8.23;px[o+3]=255;sum[0]+=px[o];sum[1]+=px[o+1];sum[2]+=px[o+2];cnt++}
 for(var i=0;i<w*h;i++)if(!px[4*i+3]){px[4*i]=sum[0]/Math.max(cnt,1);px[4*i+1]=sum[1]/Math.max(cnt,1);px[4*i+2]=sum[2]/Math.max(cnt,1);px[4*i+3]=255}
 return {w:w,h:h,rgba:px}};
/* a level file for the map's 3D view, as base64 text (lv/<n>/geo.b64: Android's bridge reads text): the same layout TRL.parse
   reads, without the sound bank before the textures and what comes after the items */
TB.geo=function(fb,L){var texAt=u32(fb,16),n=L.end-texAt,o=new Uint8Array(20+n);o.set(fb.subarray(0,20));o.set(fb.subarray(texAt,L.end),20);o[16]=20;o[17]=o[18]=o[19]=0;
 var A='ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/',parts=[];
 for(var i=0;i<o.length;i+=3072){var s='';for(var j=i;j<Math.min(i+3072,o.length);j+=3){var a=o[j],b=o[j+1],c=o[j+2],v=(a<<16)|((b||0)<<8)|(c||0);
   s+=A[v>>18]+A[(v>>12)&63]+(j+1<o.length?A[(v>>6)&63]:'=')+(j+2<o.length?A[v&63]:'=')}parts.push(s)}
 return parts.join('')};
/* 15-bit PlayStation picture (loading screens) as RGBA */
function pic15(u,w){var h=u.length/2/w|0,px=new Uint8ClampedArray(w*h*4);for(var i=0;i<w*h;i++){var c=u[2*i]|u[2*i+1]<<8;px[4*i]=(c&31)*8.23;px[4*i+1]=((c>>5)&31)*8.23;px[4*i+2]=((c>>10)&31)*8.23;px[4*i+3]=255}return {w:w,h:h,rgba:px}}

/* ---------------- the whole build ---------------- */
/* job: {stop, done, total, phase} updated as it goes; breathe() lets the page redraw between rooms */
TB.build=async function(reader,size,put,enc,ext,job,breathe,discName){breathe=breathe||function(){return Promise.resolve()};
 job.phase='Opening the disc';var d=await TB.open(reader,size);
 job.phase='Names';var names=TB.names(await d.file(d.exe));await put('names.json',JSON.stringify(names));
 job.phase='Pictures';var title=await d.file('DELDATA/AMERTIT.RAW');if(title){var t=pic15(RNC.unpack(title),384);await put('img/title.'+ext,await enc(t.w,t.h,t.rgba))}
 for(var a in TB.AREAS){var r=await d.file('DELDATA/'+TB.AREAS[a]+'.RAW');if(!r)continue;var p=pic15(RNC.unpack(r),384);await put('img/area_'+a+'.'+ext,await enc(p.w,p.h,p.rgba));await breathe()}
 var lv=[],icons={};job.total=0;var Ls=[];
 for(var n=0;n<16;n++){job.phase='Reading '+(names.levels[n]||FILES[n]);await breathe();var fb=await d.file('PSXDATA/'+FILES[n]+'.PSX'),t0=Date.now();
  if(G.TR_DEBUG)console.log('level file '+n+' '+(fb&&fb.length));var L=TRL.parse(fb);if(G.TR_DEBUG)console.log('parsed '+n+' in '+(Date.now()-t0)+'ms');await put('lv/'+n+'/geo.b64',TB.geo(fb,L));Ls.push(L);job.total+=L.rooms.length}
 job.done=0;
 for(n=0;n<16;n++){L=Ls[n];Ls[n]=null;var o=TB.level(L);o.name=names.levels[n];await put('lv/'+n+'.json',JSON.stringify(o));
  var bg=TB.floorTile(L);if(bg)await put('lv/'+n+'/bg.'+ext,await enc(bg.w,bg.h,bg.rgba));
  for(var ri=0;ri<L.rooms.length;ri++){if(job.stop)throw new Error('Stopped. Press BUILD to start again.');
   job.phase=(names.levels[n]||FILES[n])+', room '+ri;var rp=TRM.renderRoom(L,ri);await put('lv/'+n+'/'+ri+'.'+ext,await enc(rp.w,rp.h,rp.rgba));
   /* tall rooms with floors at several heights: one more picture per lower level, cut above it (rooms[ri].cuts) */
   var cuts=TRM.cuts(L.rooms[ri]);o.rooms[ri].cuts=cuts;
   for(var ci=0;ci<cuts.length;ci++){var cp=TRM.renderRoom(L,ri,0,cuts[ci]);await put('lv/'+n+'/'+ri+'_'+ci+'.'+ext,await enc(cp.w,cp.h,cp.rgba))}
   job.done++;await breathe()}
  await put('lv/'+n+'.json',JSON.stringify(o));   /* again, now with the cuts */
  /* guns, ammo and medi packs look the same everywhere; puzzle items, keys and scion pieces are drawn per level */
  TB.KEY.concat([88,89,90,91,93,94]).forEach(function(ty){var own=TB.PERLEVEL.indexOf(ty)>=0,k=own?n+'_'+ty:String(ty);if(icons[k])return;var ic=TB.icon(L,ty);if(ic)icons[k]=ic});
  lv.push(L.rooms.length)}
 job.phase='Icons';for(var k in icons)await put('icons/'+k+'.'+ext,await enc(icons[k].w,icons[k].h,icons[k].rgba));
 await put('manifest.json',JSON.stringify({v:TB.VERSION,t:Date.now(),disc:discName||'',ext:ext,levels:lv,icons:Object.keys(icons)}));
 job.phase='Done'};
G.TB=TB;if(typeof module!=='undefined')module.exports=TB;
})(typeof window!=='undefined'?window:globalThis);
