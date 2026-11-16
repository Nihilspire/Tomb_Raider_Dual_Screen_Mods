/* The game's own lettering as a real font, so all the page's text can use it: TRFONT.build(L) takes a level parsed by TRL.parse and returns a
   TrueType file (Uint8Array). The letters are the level's font sprites (sequence 190, 16 pixels to the em), and each glyph is made of squares
   for its lit pixels: the dark edge they have in the game comes from the page's text-shadow, so text can still take any colour.
   Nothing about the letters is stored in the app: they come from the player's disc. Punctuation the game hasn't got is drawn below in its style. */
(function(G){
'use strict';
var TRFONT={};
var UPM=1024,PX=64;
/* sprite index for each character (the game's sequence: A-Z, a-z, 0-9, then punctuation) */
function map(){var m={},i;
 for(i=0;i<26;i++){m[65+i]=i;m[97+i]=26+i}for(i=0;i<10;i++)m[48+i]=52+i;
 var p={'.':62,',':63,'!':64,'?':65,'[':66,'"':67,'/':68,'^':69,'-':71,'+':72,':':73,'ß':74,']':75,'\\':76,'`':77,'#':78,"'":79};
 for(var k in p)m[k.charCodeAt(0)]=p[k];return m}
/* drawn here, top row 11 pixels above the baseline; # is lit */
var EXTRA={'(':['..#','.#.','#..','#..','#..','#..','#..','#..','.#.','..#'],')':['#..','.#.','..#','..#','..#','..#','..#','..#','.#.','#..'],
 ';':['##','##','..','..','..','##','##','.#','#.'],'=':['#####','.....','#####'],'*':['#.#.#','.###.','#.#.#'],'<':['...#','..#.','.#..','#...','.#..','..#.','...#'],'>':['#...','.#..','..#.','...#','..#.','.#..','#...'],
 '%':['##...#','##..#.','...#..','..#...','.#....','#...##','....##'],'&':['.##...','#..#..','#..#..','.##...','#.#..#','#..##.','#..#..','.##.##'],'_':['######'],'·':['##','##']};
var EXTRA_TOP={'(':10,')':10,';':7,'=':6,'*':10,'<':8,'>':8,'%':9,'&':9,'_':-1,'·':5};
/* a glyph from rows of lit pixels: squares, joined where a row repeats. top = pixels above the baseline of the first row */
function shape(rows,top,w){var rects=[],open={};
 for(var y=0;y<=rows.length;y++){var next={},row=rows[y]||[];
  for(var x=0;x<w;){if(!row[x]){x++;continue}var e=x;while(e<w&&row[e])e++;var k=x+'_'+e;next[k]=open[k]||{x0:x,x1:e,y0:y,y1:y+1};next[k].y1=y+1;delete open[k];x=e}
  for(var o in open)rects.push(open[o]);open=next}
 return {rects:rects.map(function(r){return {x0:r.x0*PX,x1:r.x1*PX,y1:(top-r.y0)*PX,y0:(top-r.y1)*PX}}),adv:(w+1)*PX}}
function fromSprite(L,t){var w=t.u1-t.u0+1,h=t.v1-t.v0+1,rows=[],lo=w,hi=-1;
 for(var y=0;y<h;y++){var r=[];for(var x=0;x<w;x++){var c=TRL.texel(L,t.tile,t.clut,t.u0+x,t.v0+y),on=false;
   if(c){var a=(c&31)*8,b=((c>>5)&31)*8,d=((c>>10)&31)*8;on=a+b+d>230}   /* the lit part, not its dark edge */
   r[x]=on;if(on){if(x<lo)lo=x;if(x>hi)hi=x}}rows.push(r)}
 if(hi<0)return {rects:[],adv:6*PX};
 return shape(rows.map(function(r){return r.slice(lo,hi+1)}),-t.t,hi-lo+1)}
function fromText(rows,top){return shape(rows.map(function(s){return s.split('').map(function(c){return c==='#'})}),top,rows[0].length)}
/* the TrueType file */
function W(){var a=[];return {a:a,u8:function(v){a.push(v&255)},u16:function(v){a.push((v>>8)&255,v&255)},u32:function(v){a.push((v>>>24)&255,(v>>16)&255,(v>>8)&255,v&255)},n:function(){return a.length}}}
function file(glyphs,codes){var i,g,glyf=W(),loca=W(),hmtx=W();
 glyphs.forEach(function(q){loca.u32(glyf.n());hmtx.u16(q.adv);var xs=q.rects.map(function(r){return r.x0});hmtx.u16(xs.length?Math.min.apply(null,xs):0);
  if(!q.rects.length)return;var x0=1e9,x1=-1e9,y0=1e9,y1=-1e9;q.rects.forEach(function(r){x0=Math.min(x0,r.x0);x1=Math.max(x1,r.x1);y0=Math.min(y0,r.y0);y1=Math.max(y1,r.y1)});
  glyf.u16(q.rects.length);glyf.u16(x0);glyf.u16(y0);glyf.u16(x1);glyf.u16(y1);q.rects.forEach(function(r,k){glyf.u16(k*4+3)});glyf.u16(0);
  var pts=[];q.rects.forEach(function(r){pts.push([r.x0,r.y0],[r.x0,r.y1],[r.x1,r.y1],[r.x1,r.y0])});   /* clockwise */
  pts.forEach(function(){glyf.u8(1)});var px=0;pts.forEach(function(p){glyf.u16(p[0]-px);px=p[0]});var py=0;pts.forEach(function(p){glyf.u16(p[1]-py);py=p[1]});
  while(glyf.n()%4)glyf.u8(0)});
 loca.u32(glyf.n());
 var head=W();head.u32(0x10000);head.u32(0x10000);head.u32(0);head.u32(0x5F0F3CF5);head.u16(3);head.u16(UPM);for(i=0;i<4;i++)head.u32(0);head.u16(0);head.u16(-8*PX);head.u16(24*PX);head.u16(14*PX);head.u16(0);head.u16(8);head.u16(2);head.u16(1);head.u16(0);
 var ASC=15*PX,DESC=3*PX,hhea=W();   /* capitals, 12 pixels tall, sit in the middle of a line */hhea.u32(0x10000);hhea.u16(ASC);hhea.u16(-DESC);hhea.u16(0);hhea.u16(24*PX);hhea.u16(0);hhea.u16(0);hhea.u16(24*PX);hhea.u16(1);hhea.u16(0);hhea.u16(0);for(i=0;i<5;i++)hhea.u16(0);hhea.u16(glyphs.length);
 var maxPts=0,maxC=0;glyphs.forEach(function(q){maxPts=Math.max(maxPts,q.rects.length*4);maxC=Math.max(maxC,q.rects.length)});
 var maxp=W();maxp.u32(0x10000);maxp.u16(glyphs.length);maxp.u16(maxPts);maxp.u16(maxC);maxp.u16(0);maxp.u16(0);maxp.u16(2);maxp.u16(0);maxp.u16(0);maxp.u16(0);maxp.u16(0);maxp.u16(0);maxp.u16(0);maxp.u16(0);maxp.u16(0);
 var os2=W();os2.u16(3);os2.u16(8*PX);os2.u16(400);os2.u16(5);os2.u16(0);for(i=0;i<10;i++)os2.u16(100);os2.u16(0);for(i=0;i<10;i++)os2.u8(0);for(i=0;i<4;i++)os2.u32(0);'NONE'.split('').forEach(function(c){os2.u8(c.charCodeAt(0))});os2.u16(0x40);os2.u16(codes[0]);os2.u16(Math.min(codes[codes.length-1],0xFFFF));os2.u16(ASC);os2.u16(-DESC);os2.u16(0);os2.u16(ASC+PX*2);os2.u16(DESC+PX*2);os2.u32(1);os2.u32(0);os2.u16(10*PX);os2.u16(12*PX);os2.u16(0);os2.u16(32);os2.u16(0);
 var nm=['Tomb Raider Game','Regular','Tomb Raider Game','TombRaiderGame'],ids=[1,2,4,6],str=[],name=W();name.u16(0);name.u16(4);name.u16(6+12*4);var off=0;
 nm.forEach(function(s,k){name.u16(3);name.u16(1);name.u16(0x409);name.u16(ids[k]);name.u16(s.length*2);name.u16(off);off+=s.length*2});nm.forEach(function(s){s.split('').forEach(function(c){name.u16(c.charCodeAt(0))})});
 var post=W();post.u32(0x30000);for(i=0;i<7;i++)post.u32(0);
 /* cmap: format 4 with one segment per character, simple and small enough */
 var segs=codes.length+1,cm=W();cm.u16(0);cm.u16(1);cm.u16(3);cm.u16(1);cm.u32(12);cm.u16(4);cm.u16(16+8*segs);cm.u16(0);cm.u16(segs*2);var es=0;while((2<<es)<=segs)es++;cm.u16(2<<es);cm.u16(es);cm.u16(segs*2-(2<<es));
 codes.forEach(function(c){cm.u16(c)});cm.u16(0xFFFF);cm.u16(0);codes.forEach(function(c){cm.u16(c)});cm.u16(0xFFFF);codes.forEach(function(c,k){cm.u16((k+1-c)&0xFFFF)});cm.u16(1);for(i=0;i<segs;i++)cm.u16(0);
 var T=[['OS/2',os2],['cmap',cm],['glyf',glyf],['head',head],['hhea',hhea],['hmtx',hmtx],['loca',loca],['maxp',maxp],['name',name],['post',post]],out=W();
 var es2=0;while((2<<es2)<=T.length)es2++;out.u32(0x10000);out.u16(T.length);out.u16(16<<es2);out.u16(es2);out.u16(T.length*16-(16<<es2));
 function sum(a){var s=0;for(var k=0;k<a.length;k+=4)s=(s+((a[k]<<24)|((a[k+1]||0)<<16)|((a[k+2]||0)<<8)|(a[k+3]||0)))>>>0;return s}
 var at=12+16*T.length;T.forEach(function(t){var a=t[1].a;t[0].split('').forEach(function(c){out.u8(c.charCodeAt(0))});out.u32(sum(a));out.u32(at);out.u32(a.length);at+=(a.length+3)&~3});
 T.forEach(function(t){t[1].a.forEach(function(v){out.u8(v)});while(out.n()%4)out.u8(0)});
 var adj=(0xB1B0AFBA-sum(out.a))>>>0,hAt=12+16*T.length;T.slice(0,3).forEach(function(t){hAt+=(t[1].a.length+3)&~3});
 out.a[hAt+8]=adj>>>24;out.a[hAt+9]=(adj>>16)&255;out.a[hAt+10]=(adj>>8)&255;out.a[hAt+11]=adj&255;
 return new Uint8Array(out.a)}
TRFONT.build=function(L){var q=null;L.sseq.forEach(function(s){if(s.type===190)q=s});if(!q)return null;
 var m=map(),cs={},k;for(k in m){var t=L.stex[q.start+m[k]];if(t)cs[k]=fromSprite(L,t)}
 for(k in EXTRA)cs[k.charCodeAt(0)]=fromText(EXTRA[k],EXTRA_TOP[k]);
 cs[32]={rects:[],adv:6*PX};cs[160]=cs[32];
 var codes=Object.keys(cs).map(Number).sort(function(a,b){return a-b}),glyphs=[{rects:[],adv:8*PX}];codes.forEach(function(c){glyphs.push(cs[c])});
 return file(glyphs,codes)};
G.TRFONT=TRFONT;if(typeof module!=='undefined')module.exports=TRFONT;
})(typeof window!=='undefined'?window:globalThis);
