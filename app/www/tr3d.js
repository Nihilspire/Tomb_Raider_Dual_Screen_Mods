/* The rooms of a Tomb Raider level in 3D with WebGL, shared by the map's 3D view (index.html) and the story step editor (editor.html).
   Game coordinates: x east, z north, y down. The view uses (x, -y, -z). A face is drawn from its texture page and palette as the
   PlayStation does: the 4-bit texel index from the page, its colour from the palette, colour 0 see-through; the texel is kept
   inside the face's own corner of the page. Faces are one-sided, seen from inside their room: from above, the ceilings and the near
   walls drop away. */
(function(G){
'use strict';
var TR3D={FOV:0.9};
function sub(a,b){return [a[0]-b[0],a[1]-b[1],a[2]-b[2]]}
function cross(a,b){return [a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]]}
function norm(a){var l=Math.hypot(a[0],a[1],a[2])||1;return [a[0]/l,a[1]/l,a[2]/l]}
function dot(a,b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]}
function mul(a,b){var o=new Array(16);for(var c=0;c<4;c++)for(var r=0;r<4;r++){var s=0;for(var k=0;k<4;k++)s+=a[k*4+r]*b[c*4+k];o[c*4+r]=s}return o}
/* a camera turning round the point t (view coordinates): {VP, eye, f, r, u, proj}. proj(x, y, z) puts a game point on the
   screen as [x, y] in CSS pixels, or null behind the camera */
TR3D.camera=function(t,yaw,pitch,dist,W,H){var c=Math.cos(pitch),e=[t[0]+dist*c*Math.sin(yaw),t[1]+dist*Math.sin(pitch),t[2]+dist*c*Math.cos(yaw)];
 var f=norm(sub(t,e)),r=norm(cross(f,[0,1,0])),u=cross(r,f),near=40,far=400000,q=1/Math.tan(TR3D.FOV/2),a=W/H;
 var view=[r[0],u[0],-f[0],0,r[1],u[1],-f[1],0,r[2],u[2],-f[2],0,-dot(r,e),-dot(u,e),dot(f,e),1];
 var m=mul([q/a,0,0,0,0,q,0,0,0,0,(far+near)/(near-far),-1,0,0,2*far*near/(near-far),0],view);
 return {VP:m,eye:e,f:f,r:r,u:u,proj:function(x,y,z){var X=x,Y=-y,Z=-z,w=m[3]*X+m[7]*Y+m[11]*Z+m[15];if(w<=1)return null;
  return [((m[0]*X+m[4]*Y+m[8]*Z+m[12])/w*0.5+0.5)*W,(0.5-(m[1]*X+m[5]*Y+m[9]*Z+m[13])/w*0.5)*H]}}};
/* create(canvas, {antialias, alpha}) -> the renderer, or null without WebGL */
TR3D.create=function(canvas,opts){opts=opts||{};
 var gl=canvas.getContext('webgl',{antialias:!!opts.antialias,alpha:!!opts.alpha});if(!gl)return null;
 function sh(type,src){var s=gl.createShader(type);gl.shaderSource(s,src);gl.compileShader(s);if(!gl.getShaderParameter(s,gl.COMPILE_STATUS))throw new Error(gl.getShaderInfoLog(s));return s}
 var PR=gl.createProgram();
 gl.attachShader(PR,sh(gl.VERTEX_SHADER,'attribute vec3 aP;attribute vec2 aUV;attribute vec4 aB;attribute vec2 aTC;attribute vec3 aC;'
  +'uniform mat4 uM;varying vec2 vUV;varying vec4 vB;varying vec2 vTC;varying vec3 vC;varying float vY;'
  +'void main(){vUV=aUV;vB=aB;vTC=aTC;vC=aC;vY=aP.y;gl_Position=uM*vec4(aP.x,-aP.y,-aP.z,1.0);}'));
 /* the 13 pages sit in a 4 x 4 grid of 256 x 256 in one 1024 x 1024 texture (some phones allow no more than 2048 on a side) */
 gl.attachShader(PR,sh(gl.FRAGMENT_SHADER,'precision highp float;uniform sampler2D uTiles;uniform sampler2D uClut;uniform float uCut;'
  +'varying vec2 vUV;varying vec4 vB;varying vec2 vTC;varying vec3 vC;varying float vY;'
  +'void main(){if(vY<uCut)discard;vec2 t=clamp(floor(vUV),vB.xy,vB.zw);float p=floor(vTC.x+0.5),row=floor(p/4.0),col=p-row*4.0;'
  +'float ix=texture2D(uTiles,vec2((col*256.0+t.x+0.5)/1024.0,(row*256.0+t.y+0.5)/1024.0)).r*255.0;'
  +'vec4 c=texture2D(uClut,vec2((floor(ix+0.5)+0.5)/16.0,(floor(vTC.y+0.5)+0.5)/1024.0));if(c.a<0.5)discard;'
  +'gl_FragColor=vec4(c.rgb*vC,1.0);}'));
 gl.linkProgram(PR);gl.useProgram(PR);
 var AT={};['aP','aUV','aB','aTC','aC'].forEach(function(n){AT[n]=gl.getAttribLocation(PR,n);gl.enableVertexAttribArray(AT[n])});
 var UN={};['uM','uTiles','uClut','uCut'].forEach(function(n){UN[n]=gl.getUniformLocation(PR,n)});
 var TX1=gl.createTexture(),TX2=gl.createTexture(),VB=gl.createBuffer();
 function texParams(){gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_MIN_FILTER,gl.NEAREST);gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_MAG_FILTER,gl.NEAREST);
  gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_WRAP_S,gl.CLAMP_TO_EDGE);gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_WRAP_T,gl.CLAMP_TO_EDGE)}
 var R={gl:gl,nv:0,ranges:{},front:gl.CW};
 R.textures=function(L){var idx=new Uint8Array(1024*1024);
  for(var p=0;p<13;p++){var ox=(p%4)*256,oy=(p>>2)*256;for(var y=0;y<256;y++)for(var x=0;x<256;x++){var b=L.tiles[p*32768+y*128+(x>>1)];idx[(oy+y)*1024+ox+x]=x&1?b>>4:b&15}}
  gl.bindTexture(gl.TEXTURE_2D,TX1);gl.pixelStorei(gl.UNPACK_ALIGNMENT,1);gl.texImage2D(gl.TEXTURE_2D,0,gl.LUMINANCE,1024,1024,0,gl.LUMINANCE,gl.UNSIGNED_BYTE,idx);texParams();
  var cl=new Uint8Array(16*1024*4);for(var i=0;i<16*1024;i++){var c=L.cluts[i];cl[4*i]=(c&31)*8.23;cl[4*i+1]=((c>>5)&31)*8.23;cl[4*i+2]=((c>>10)&31)*8.23;cl[4*i+3]=c?255:0}
  gl.bindTexture(gl.TEXTURE_2D,TX2);gl.texImage2D(gl.TEXTURE_2D,0,gl.RGBA,16,1024,0,gl.RGBA,gl.UNSIGNED_BYTE,cl);texParams()};
 /* every room's faces in one buffer: position (3), texel (2), the face's texel box (4), page and palette (2), colour (3). With
    flip on, each room that has an alternate is drawn as it, under the normal room's number. onTri(room, shown room record,
    the triangle's 9 numbers), if given, is told each triangle (the editor's clicking) */
 R.rooms=function(L,flip,onTri){var alt={};L.rooms.forEach(function(r,i){if(r.alt>=0)alt[r.alt]=i});
  var tot=0;L.rooms.forEach(function(r0,ri){if(alt[ri]!=null)return;var m=L.rooms[flip&&r0.alt>=0?r0.alt:ri];tot+=m.quads.length*6+m.tris.length*3});
  var D=new Float32Array(tot*14),o=0;R.ranges={};
  L.rooms.forEach(function(r0,ri){if(alt[ri]!=null)return;var m=L.rooms[flip&&r0.alt>=0?r0.alt:ri],V=m.V,wat=m.flags&1,start=o/14;
   function face(ix,tex){var ot=L.otex[tex&0x7FFF];if(!ot)return;var uv=ot.uv,n=ix.length,u0=255,v0=255,u1=0,v1=0;
    for(var k=0;k<n;k++){u0=Math.min(u0,uv[2*k]);u1=Math.max(u1,uv[2*k]);v0=Math.min(v0,uv[2*k+1]);v1=Math.max(v1,uv[2*k+1])}
    var tri=n===4?[0,1,2,0,2,3]:[0,1,2],pts=onTri?[]:null;
    for(var j=0;j<tri.length;j++){var q=tri[j],i=ix[q],x=V[3*i]+m.x,y=V[3*i+1],z=V[3*i+2]+m.z,li=Math.min(1.6,m.light[i]/128);
     /* the texel coordinates stretched to the face's outer edge, so the floor() in the shader lands inside it */
     D[o++]=x;D[o++]=y;D[o++]=z;D[o++]=uv[2*q]+(uv[2*q]===u1&&u1>u0?0.999:0);D[o++]=uv[2*q+1]+(uv[2*q+1]===v1&&v1>v0?0.999:0);
     D[o++]=u0;D[o++]=v0;D[o++]=u1;D[o++]=v1;D[o++]=ot.tile;D[o++]=ot.clut;D[o++]=li*(wat?0.65:1);D[o++]=li*(wat?0.85:1);D[o++]=li*(wat?1.15:1);
     if(pts){pts.push(x,y,z);if(pts.length===9){onTri(ri,m,pts);pts=[]}}}}
   m.quads.forEach(function(q){face([q[0],q[1],q[2],q[3]],q[4])});m.tris.forEach(function(t){face([t[0],t[1],t[2]],t[3])});
   R.ranges[ri]=[start,o/14-start]});
  R.nv=o/14;gl.bindBuffer(gl.ARRAY_BUFFER,VB);gl.bufferData(gl.ARRAY_BUFFER,D.subarray(0,o),gl.STATIC_DRAW)};
 /* draw(camera, width and height in device pixels, cut, visible, clear): leaves out what is above height cut (null: nothing);
    visible(room), if given, picks the rooms */
 R.draw=function(cam,pw,ph,cut,visible,clear){gl.viewport(0,0,pw,ph);gl.clearColor(clear[0],clear[1],clear[2],clear[3]);gl.clear(gl.COLOR_BUFFER_BIT|gl.DEPTH_BUFFER_BIT);if(!R.nv)return;
  gl.enable(gl.DEPTH_TEST);gl.enable(gl.CULL_FACE);gl.cullFace(gl.BACK);gl.frontFace(R.front);
  gl.useProgram(PR);gl.uniformMatrix4fv(UN.uM,false,new Float32Array(cam.VP));gl.uniform1f(UN.uCut,cut==null?-1e9:cut);
  gl.activeTexture(gl.TEXTURE0);gl.bindTexture(gl.TEXTURE_2D,TX1);gl.uniform1i(UN.uTiles,0);gl.activeTexture(gl.TEXTURE1);gl.bindTexture(gl.TEXTURE_2D,TX2);gl.uniform1i(UN.uClut,1);
  gl.bindBuffer(gl.ARRAY_BUFFER,VB);var S=56;gl.vertexAttribPointer(AT.aP,3,gl.FLOAT,false,S,0);gl.vertexAttribPointer(AT.aUV,2,gl.FLOAT,false,S,12);
  gl.vertexAttribPointer(AT.aB,4,gl.FLOAT,false,S,20);gl.vertexAttribPointer(AT.aTC,2,gl.FLOAT,false,S,36);gl.vertexAttribPointer(AT.aC,3,gl.FLOAT,false,S,44);
  if(!visible){gl.drawArrays(gl.TRIANGLES,0,R.nv);return}
  for(var k in R.ranges)if(visible(+k)&&R.ranges[k][1])gl.drawArrays(gl.TRIANGLES,R.ranges[k][0],R.ranges[k][1])};
 return R};
G.TR3D=TR3D;if(typeof module!=='undefined')module.exports=TR3D;
})(typeof window!=='undefined'?window:globalThis);
