/* Rob Northen ProPack method 2 unpacker (the loading screens in DELDATA/*.RAW). Header: "RNC" 2, unpacked size (u32 BE),
   packed size (u32 BE), two CRCs, leeway, chunk count; then a bit stream read a byte at a time, high bit first. */
(function(G){
'use strict';
function unpack(src){
 if(src[0]!==0x52||src[1]!==0x4E||src[2]!==0x43||src[3]!==2)throw new Error('not RNC method 2');
 var size=(src[4]<<24|src[5]<<16|src[6]<<8|src[7])>>>0,out=new Uint8Array(size),o=0,p=18,bb=0,bc=0;
 function bit(){if(!bc){bb=src[p++];bc=8}var v=(bb>>7)&1;bb=(bb<<1)&0xFF;bc--;return v}
 bit();bit();
 for(;;){
  while(!bit()){if(o>=size)return out;out[o++]=src[p++]}
  var len=2,hi=0,load;
  if(!bit()){
   len=(len<<1)|bit();
   if(bit()){len--;len=(len<<1)|bit();
    if(len===9){len=4;while(len--)hi=(hi<<1)|bit();len=(hi+3)*4;while(len--)out[o++]=src[p++];continue}}
   load=true}
  else{
   if(bit()){len++;if(bit()){len=src[p++];if(!len){if(bit())continue;break}len+=8}load=true}
   else load=false}
  if(load&&bit()){hi=(hi<<1)|bit();
   if(bit()){hi=((hi<<1)|bit())|4;if(!bit())hi=(hi<<1)|bit()}
   else if(hi===0)hi=2|bit()}
  var off=((hi<<8)|src[p++])+1;
  while(len--){out[o]=out[o-off];o++}}
 return out}
var RNC={unpack:unpack};G.RNC=RNC;if(typeof module!=='undefined')module.exports=RNC;
})(typeof window!=='undefined'?window:globalThis);
