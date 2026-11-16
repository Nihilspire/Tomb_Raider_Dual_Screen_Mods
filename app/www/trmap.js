/* Top-down textured room pictures for the mini map, drawn from the level's own room geometry and textures.
   A port of Xenogears Link's room picture renderer (discbuild.js render1: a software rasteriser filling triangles by barycentric
   weights, texels looked up through the PlayStation palette, colour 0 see-through, a height buffer so the topmost floor wins)
   to Tomb Raider rooms. Differences: Tomb Raider rooms are boxes of 1024-unit sectors, so every room is drawn on its own,
   north (+z) up, at PPS pixels per sector; floors are the faces that face up and sit nearer the sector's floor height than its
   ceiling; walls are drawn as dark edges over the floors. */
(function(G){
'use strict';
var TRM={PPS:32};
function clamp(v){return v<0?0:v>255?255:v}
/* renderRoom(L, ri, pps, cut) -> {w, h, rgba, x0, z0, x1, z1}: x0/z0 is the room's south-west corner in world units.
   cut (optional): leave out every face lying wholly above that height (y < cut; y points down), so a tall room can be seen
   from its lower floors without the floors and bridges overhead */
TRM.renderRoom=function(L,ri,pps,cut){pps=pps||TRM.PPS;var hasCut=cut!=null;var m=L.rooms[ri],S=1024/pps,w=m.xs*pps,h=m.zs*pps,n=w*h;
 var zb=new Float32Array(n).fill(1e9),col=new Uint8ClampedArray(n*4),V=m.V,X0=m.x,Z1=m.z+m.zs*1024;
 function sx(i){return (V[3*i])/S}function sy(i){return (m.zs*1024-V[3*i+2])/S}
 function secAt(x,z){var cx=Math.floor(x/1024),cz=Math.floor(z/1024);if(cx<0||cz<0||cx>=m.xs||cz>=m.zs)return null;return m.sec[cx*m.zs+cz]}
 var walls=[];
 function face(ix,tex){var ot=L.otex[tex&0x7FFF];if(!ot)return;
  if(hasCut){var low=-1e9;for(var q0=0;q0<ix.length;q0++)low=Math.max(low,V[3*ix[q0]+1]);if(low<cut)return}
  var a=ix[0],b=ix[1],c=ix[2],ux=V[3*b]-V[3*a],uy=V[3*b+1]-V[3*a+1],uz=V[3*b+2]-V[3*a+2],vx=V[3*c]-V[3*a],vy=V[3*c+1]-V[3*a+1],vz=V[3*c+2]-V[3*a+2];
  var nx=uy*vz-uz*vy,ny=uz*vx-ux*vz,nz=ux*vy-uy*vx,nl=Math.sqrt(nx*nx+ny*ny+nz*nz)||1;
  if(Math.abs(ny)/nl<0.35){walls.push(ix);return}   /* steep: a wall */
  var my=0,mx=0,mz=0;for(var k=0;k<ix.length;k++){my+=V[3*ix[k]+1];mx+=V[3*ix[k]];mz+=V[3*ix[k]+2]}my/=ix.length;mx/=ix.length;mz/=ix.length;
  var s=secAt(mx,mz);if(s){var fl=s.floor*256,ce=s.ceil*256;if(Math.abs(my-ce)<Math.abs(my-fl)-64)return}   /* a ceiling */
  var up=Math.abs(ny)/nl,tris=ix.length===4?[[0,1,2],[0,2,3]]:[[0,1,2]],uv=ot.uv;
  for(var t=0;t<tris.length;t++){var q=tris[t];tri(ix[q[0]],ix[q[1]],ix[q[2]],[uv[2*q[0]],uv[2*q[0]+1]],[uv[2*q[1]],uv[2*q[1]+1]],[uv[2*q[2]],uv[2*q[2]+1]],ot,up)}}
 function tri(a,b,c,ta,tb,tc,ot,up){var x0=sx(a),y0=sy(a),x1=sx(b),y1=sy(b),x2=sx(c),y2=sy(c);
  var bx0=Math.max(0,Math.floor(Math.min(x0,x1,x2))),bx1=Math.min(w-1,Math.ceil(Math.max(x0,x1,x2))),by0=Math.max(0,Math.floor(Math.min(y0,y1,y2))),by1=Math.min(h-1,Math.ceil(Math.max(y0,y1,y2)));
  var den=(y1-y2)*(x0-x2)+(x2-x1)*(y0-y2);if(Math.abs(den)<1e-6)return;var e=0.5/Math.max(Math.sqrt(Math.abs(den)),1);
  var la=m.light[a],lb=m.light[b],lc=m.light[c],za=V[3*a+1],zb2=V[3*b+1],zc=V[3*c+1],sh=0.8+0.2*up;
  for(var py=by0;py<=by1;py++)for(var px=bx0;px<=bx1;px++){var gx=px+0.5,gy=py+0.5,A=((y1-y2)*(gx-x2)+(x2-x1)*(gy-y2))/den,B=((y2-y0)*(gx-x2)+(x0-x2)*(gy-y2))/den,C=1-A-B;
   if(A<-e||B<-e||C<-e)continue;var k=py*w+px,zz=A*za+B*zb2+C*zc;if(zz>=zb[k])continue;
   var u=Math.min(255,Math.max(0,Math.round(A*ta[0]+B*tb[0]+C*tc[0]))),v=Math.min(255,Math.max(0,Math.round(A*ta[1]+B*tb[1]+C*tc[1])));
   var bt=L.tiles[ot.tile*32768+v*128+(u>>1)],c16=L.cluts[ot.clut*16+((u&1)?bt>>4:bt&15)];if(!c16)continue;
   var li=(A*la+B*lb+C*lc)/128*sh;zb[k]=zz;var o=k*4;
   col[o]=clamp((c16&31)*8.23*li);col[o+1]=clamp(((c16>>5)&31)*8.23*li);col[o+2]=clamp(((c16>>10)&31)*8.23*li);col[o+3]=255}}
 m.quads.forEach(function(q){face([q[0],q[1],q[2],q[3]],q[4])});
 m.tris.forEach(function(t){face([t[0],t[1],t[2]],t[3])});
 /* water rooms: a blue wash */
 if(m.flags&1)for(var i=0;i<n;i++)if(col[4*i+3]){col[4*i]*=0.55;col[4*i+1]*=0.8;col[4*i+2]=clamp(col[4*i+2]*0.8+70)}
 /* walls: the top-down trace of each steep face, as a dark line, only where it stands at the floor's edge */
 walls.forEach(function(ix){var pts=[];for(var k=0;k<ix.length;k++){var p=[sx(ix[k]),sy(ix[k])],dup=false;for(var j=0;j<pts.length;j++)if(Math.abs(pts[j][0]-p[0])<0.6&&Math.abs(pts[j][1]-p[1])<0.6)dup=true;if(!dup)pts.push(p)}
  if(pts.length<2)return;var A=pts[0],B=pts[1],dmax=0;for(var i2=0;i2<pts.length;i2++)for(var j2=i2+1;j2<pts.length;j2++){var d=Math.hypot(pts[i2][0]-pts[j2][0],pts[i2][1]-pts[j2][1]);if(d>dmax){dmax=d;A=pts[i2];B=pts[j2]}}
  line(A[0],A[1],B[0],B[1])});
 function line(ax,ay,bx,by){var len=Math.max(1,Math.ceil(Math.hypot(bx-ax,by-ay)*1.5));
  for(var s=0;s<=len;s++){var x=Math.floor(ax+(bx-ax)*s/len),y=Math.floor(ay+(by-ay)*s/len);
   for(var oy=-1;oy<=0;oy++)for(var ox=-1;ox<=0;ox++){var px=x+ox,py=y+oy;if(px<0||py<0||px>=w||py>=h)continue;var o=(py*w+px)*4;
    if(col[o+3]){col[o]*=0.35;col[o+1]*=0.35;col[o+2]*=0.35}else{col[o]=20;col[o+1]=16;col[o+2]=12;col[o+3]=200}}}}
 return {w:w,h:h,rgba:col,x0:m.x,z0:m.z,x1:m.x+m.xs*1024,z1:Z1}};
/* the heights to cut a tall room at: its sectors' floor heights grouped into levels at least 2 blocks apart; for every level
   below the top one, a cut 1.75 blocks above that level's highest floor (about Lara's height plus a step). [] for rooms
   less than 4 blocks tall or with one level */
TRM.cuts=function(m){if(m.yBottom-m.yTop<4096)return [];var f={};m.sec.forEach(function(s){if(s.floor!==-127&&s.floor!==s.ceil)f[s.floor*256]=1});
 var ys=Object.keys(f).map(Number).sort(function(a,b){return a-b}),groups=[];
 ys.forEach(function(y){if(!groups.length||y-groups[groups.length-1].last>=2048)groups.push({top:y,last:y});else groups[groups.length-1].last=y});
 return groups.slice(1).map(function(g){return g.top-1792})};
G.TRM=TRM;if(typeof module!=='undefined')module.exports=TRM;
})(typeof window!=='undefined'?window:globalThis);
