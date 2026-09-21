const fs=require('fs'),path=require('path');
const {createCanvas,loadImage,GlobalFonts}=require('@napi-rs/canvas');
const fontPath=process.env.PREVIEW_FONT_PATH;
if(fontPath) GlobalFonts.registerFromPath(fontPath,'Preview UI');
const {prepare,draw}=require('./render.js');
(async()=>{
  const sheet=await loadImage(path.join(__dirname,'digits-source.png'));
  const bg=await loadImage(path.join(__dirname,'background-source.png'));
  const art=prepare(createCanvas,sheet,bg);
  const clock=createCanvas(480,320);draw(clock.getContext('2d'),art);
  fs.writeFileSync(path.join(__dirname,'clock-12-45.png'),clock.toBuffer('image/png'));
  const board=createCanvas(1000,400), c=board.getContext('2d');
  c.fillStyle='#edf2f7';c.fillRect(0,0,1000,400);
  c.fillStyle='#3c5063';c.font='22px "Preview UI"';c.fillText('Flip Glass Light • digit artwork',30,34);
  for(let n=0;n<10;n++){
    const x=35+(n%5)*185,y=55+Math.floor(n/5)*170;
    c.drawImage(art.glyphs[n],x,y);
    c.fillStyle='#526476';c.font='15px "Preview UI"';c.fillText(String(n),x+44,y+148);
  }
  fs.writeFileSync(path.join(__dirname,'digits-preview.png'),board.toBuffer('image/png'));
  for(let n=0;n<11;n++) fs.writeFileSync(path.join(__dirname,`${n===10?'colon':n}.png`),art.glyphs[n].toBuffer('image/png'));
  fs.writeFileSync(path.join(__dirname,'background.png'),art.bg.toBuffer('image/png'));
  fs.writeFileSync(path.join(__dirname,'manifest.json'),JSON.stringify({status:'approved',size:[480,320],duration_ms:1500,top:84,digit_size:[100,128],positions:[14,122,258,366],reflection_height:100,source_boxes:art.boxes},null,2)+'\n');
  console.log(JSON.stringify({exported:'clock, digit board, glyph PNGs, background and manifest',boxes:art.boxes}));
})().catch(e=>{console.error(e);process.exit(1)});
