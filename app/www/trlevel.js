/* Tomb Raider (PlayStation, US) level reader: PSXDATA/LEVEL*.PSX from the player's own disc.
   Layout as read by OpenLara (src/format.h, loadTR1_PSX); sizes checked against the US Rev 6 files.
   Gives rooms (geometry, portals, sectors), floor data, object textures, sprite textures, items and the 4-bit texture pages
   with their palettes. Runs in the app page and under Node for the tools. */
(function(G){
'use strict';
function R(b){var dv=new DataView(b.buffer,b.byteOffset,b.byteLength),p=0;return {
 get pos(){return p},set pos(v){p=v},
 u8:function(){return b[p++]},s8:function(){var v=b[p++];return v>127?v-256:v},
 u16:function(){var v=dv.getUint16(p,true);p+=2;return v},s16:function(){var v=dv.getInt16(p,true);p+=2;return v},
 u32:function(){var v=dv.getUint32(p,true);p+=4;return v},s32:function(){var v=dv.getInt32(p,true);p+=4;return v},
 skip:function(n){p+=n},bytes:function(n){var o=b.subarray(p,p+n);p+=n;return o}}}

var TRL={};
/* parse(bytes) -> level. Throws when the file is not a TR1 PSX level. */
TRL.parse=function(b){var r=R(b),L={};
 r.pos=4;if(r.u32()!==0x56414270)throw new Error('not a Tomb Raider PlayStation level');
 r.pos=16;var texAt=r.u32();r.pos=texAt+8;
 L.tiles=r.bytes(13*32768);            /* 13 pages of 256 x 256 4-bit texels */
 L.cluts=new Uint16Array(1024*16);     /* 1024 palettes of 16 colours (PS1 15-bit, 0 = see-through) */
 for(var i=0;i<1024*16;i++)L.cluts[i]=r.u16();
 r.skip(4);
 var nr=r.u16();L.rooms=[];
 for(var ri=0;ri<nr;ri++){var m={};
  m.x=r.s32();m.z=r.s32();m.yBottom=r.s32();m.yTop=r.s32();
  var size=r.u32(),start=r.pos;r.skip(2);
  var nv=r.s16();m.V=new Int16Array(nv*3);m.light=new Uint16Array(nv);
  for(var k=0;k<nv;k++){m.V[3*k]=r.s16();m.V[3*k+1]=r.s16();m.V[3*k+2]=r.s16();m.light[k]=r.u16()}
  var nq=r.s16();m.quads=[];for(k=0;k<nq;k++){var a=r.u16(),bb=r.u16(),c=r.u16(),d=r.u16(),t=r.u16();m.quads.push([a,bb,d,c,t])}   /* strip order -> list order */
  var nt=r.s16();m.tris=[];for(k=0;k<nt;k++)m.tris.push([r.u16(),r.u16(),r.u16(),r.u16()]);
  var ns=r.s16();m.sprites=[];for(k=0;k<ns;k++)m.sprites.push([r.s16(),r.s16()]);
  r.pos=start+size*2;
  var np=r.u16();m.portals=[];
  for(k=0;k<np;k++){var pr={room:r.u16(),n:[r.s16(),r.s16(),r.s16()],v:[]};for(var j=0;j<4;j++)pr.v.push([r.s16(),r.s16(),r.s16()]);m.portals.push(pr)}
  m.zs=r.u16();m.xs=r.u16();m.sec=[];
  for(k=0;k<m.zs*m.xs;k++)m.sec.push({fd:r.u16(),box:r.u16(),below:r.u8(),floor:r.s8(),above:r.u8(),ceil:r.s8()});
  m.ambient=r.u16();
  var nl=r.u16();r.skip(nl*20);
  var nm=r.u16();m.statics=[];for(k=0;k<nm;k++){var sx=r.s32(),sy=r.s32(),sz=r.s32(),rot=r.u16();r.skip(2);var mid=r.u16();r.skip(2);m.statics.push({x:sx,y:sy,z:sz,rot:rot,id:mid})}
  m.alt=r.s16();m.flags=r.u16();
  L.rooms.push(m)}
 var nfd=r.u32();L.fd=new Uint16Array(nfd);for(i=0;i<nfd;i++)L.fd[i]=r.u16();
 var nmd=r.u32();L.meshData=r.bytes(nmd*2);
 var nmo=r.u32();L.meshOff=new Uint32Array(nmo);for(i=0;i<nmo;i++)L.meshOff[i]=r.u32();
 var na=r.u32();r.skip(na*32);
 var nst=r.u32();r.skip(nst*6);
 var nrg=r.u32();r.skip(nrg*8);
 var ncm=r.u32();r.skip(ncm*2);
 var nnd=r.u32();r.skip(nnd*4);
 var nfr=r.u32();r.skip(nfr*2);
 var nmod=r.u32();L.models=[];for(i=0;i<nmod;i++){var ty=r.u16();r.skip(2);var mc=r.u16(),ms=r.u16();r.skip(4+4+2+2);L.models.push({type:ty,count:mc,start:ms})}
 var nsm=r.u32();L.staticMeshes=[];for(i=0;i<nsm;i++){var id=r.u32(),mesh=r.u16();r.skip(24);var fl=r.u16();L.staticMeshes.push({id:id,mesh:mesh,flags:fl})}
 var not=r.u32();L.otex=[];
 for(i=0;i<not;i++){var x0=r.u8(),y0=r.u8(),cl=r.u16(),x1=r.u8(),y1=r.u8(),tile=r.u16()&0x3FFF,x2=r.u8(),y2=r.u8();r.skip(2);var x3=r.u8(),y3=r.u8(),at=r.u16();
  L.otex.push({clut:cl,tile:tile,attr:at,uv:[x0,y0,x1,y1,x2,y2,x3,y3]})}
 var nspt=r.u32();L.stex=[];
 for(i=0;i<nspt;i++){var l=r.s16(),tt=r.s16(),rr=r.s16(),bo=r.s16(),c2=r.u16(),ti=r.u16(),u0=r.u8(),v0=r.u8(),u1=r.u8(),v1=r.u8();L.stex.push({l:l,t:tt,r:rr,b:bo,clut:c2,tile:ti,u0:u0,v0:v0,u1:u1,v1:v1})}
 var nseq=r.u32();L.sseq=[];for(i=0;i<nseq;i++){var sty=r.u16();r.skip(2);var cnt=-r.s16(),st=r.s16();L.sseq.push({type:sty,count:cnt,start:st})}
 var ncam=r.u32();L.cams=[];for(i=0;i<ncam;i++){L.cams.push({x:r.s32(),y:r.s32(),z:r.s32(),room:r.s16(),flags:r.u16()})}
 var nss=r.u32();r.skip(nss*16);
 var nbox=r.u32();L.boxes=[];for(i=0;i<nbox;i++)L.boxes.push({z0:r.s32(),z1:r.s32(),x0:r.s32(),x1:r.s32(),floor:r.s16(),ov:r.u16()});
 /* overlaps: for each box, from boxes[i].ov & 0x3FFF, the boxes it touches (& 0x7FFF), bit 15 ending its list */
 var nov=r.u32();L.overlaps=new Uint16Array(nov);for(i=0;i<nov;i++)L.overlaps[i]=r.u16();
 r.skip(nbox*2*3*2);
 var nat=r.u32();r.skip(nat*2);
 var ne=r.u32();L.items=[];
 for(i=0;i<ne;i++){L.items.push({type:r.u16(),room:r.s16(),x:r.s32(),y:r.s32(),z:r.s32(),rot:r.u16(),light:r.s16(),flags:r.u16()})}
 L.end=r.pos;
 return L};

/* colour of a 15-bit PS1 texel as [r,g,b] 0..255 */
TRL.rgb=function(c){return [(c&31)*255/31|0,((c>>5)&31)*255/31|0,((c>>10)&31)*255/31|0]};
/* one texel of an object texture page: 4-bit index into the palette; returns the 15-bit colour (0 = see-through) */
TRL.texel=function(L,tile,clut,u,v){var b=L.tiles[tile*32768+v*128+(u>>1)],ix=u&1?b>>4:b&15;return L.cluts[clut*16+ix]};

/* floor data for a sector: {floor, ceil (absolute y, smaller is higher), triggers:[{type, items:[...], ...}], kill, portal room}.
   TR1 floor data: function codes 1 portal, 2 floor slope, 3 ceiling slope, 4 trigger, 5 kill; bit 15 ends the list. */
TRL.sectorInfo=function(L,s){var o={portal:-1,trig:null,kill:false,slope:null};if(!s.fd)return o;var p=s.fd,fd=L.fd;
 for(var guard=0;guard<64&&p<fd.length;guard++){var w=fd[p++],fn=w&0x1F,sub=(w>>8)&0x7F,end=w&0x8000;
  if(fn===1){o.portal=fd[p++]&0xFF}
  else if(fn===2||fn===3){if(fn===2)o.slope=fd[p];p++}
  else if(fn===4){var setup=fd[p++],t={type:sub,timer:setup&0xFF,mask:(setup>>9)&31,once:!!(setup&0x100),acts:[]};
   for(var g=0;g<64;g++){var a=fd[p++],act=(a>>10)&0x1F,par=a&0x3FF;
    if(sub===2&&g===0){t.key=par;if(a&0x8000)break;continue}   /* switch: first word is the switch item */
    if((sub===3||sub===4)&&g===0){t.key=par;if(a&0x8000)break;continue} /* key / pickup trigger: the lock or item */
    t.acts.push([act,par]);if(act===1){var c=fd[p++];if(c&0x8000)break}
    if(a&0x8000)break}
   o.trig=t}
  else if(fn===5){o.kill=true}
  if(end)break}
 return o};
G.TRL=TRL;if(typeof module!=='undefined')module.exports=TRL;
})(typeof window!=='undefined'?window:globalThis);
