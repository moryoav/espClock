const fs=require('fs'),path=require('path');
const {createCanvas,loadImage,GlobalFonts}=require('@napi-rs/canvas');
const Pizza=require('./render.js');
if(process.env.PREVIEW_FONT_PATH)GlobalFonts.registerFromPath(process.env.PREVIEW_FONT_PATH,'Preview UI');
(async()=>{
  const art=Pizza.prepare(createCanvas,await loadImage(path.join(__dirname,'pepperoni-source.png')),await loadImage(path.join(__dirname,'background-source.png')));
  const save=(canvas,name)=>fs.writeFileSync(path.join(__dirname,name),canvas.toBuffer('image/png'));
  const clock=createCanvas(480,320);Pizza.draw(clock.getContext('2d'),art);save(clock,'clock-12-45.png');save(art.bg,'background.png');
  const board=createCanvas(1000,440),b=board.getContext('2d');b.fillStyle='#fff2d8';b.fillRect(0,0,1000,440);
  b.fillStyle='#633018';b.font='24px "Preview UI", sans-serif';b.fillText('Pizza clock · pepperoni digits',28,34);
  for(let n=0;n<10;n++){
    const x=20+(n%5)*196,y=52+Math.floor(n/5)*185;
    b.save();b.beginPath();b.roundRect(x,y,176,153,10);b.clip();b.drawImage(art.bg,112,85,240,160,x,y,176,153);b.restore();
    Pizza.digit(b,art,n,x+56,y+25);
    b.fillStyle='#633018';b.font='16px "Preview UI", sans-serif';b.fillText(String(n),x+84,y+174);
  }
  save(board,'digits-preview.png');
  const strip=createCanvas(1000,180),c=strip.getContext('2d');c.fillStyle='#fff2d8';c.fillRect(0,0,1000,180);
  const labels=['Whole slice','First bite','Second bite','Third bite','Eaten'];
  for(let phase=0;phase<5;phase++){
    const x=phase*200;c.drawImage(art.bg,140,110,120,120,x+48,14,104,104);
    c.drawImage(art.sprites[0][phase],x+55,21,90,90);
    c.fillStyle='#633018';c.font='18px "Preview UI", sans-serif';c.textAlign='center';c.fillText(labels[phase],x+100,151);
  }
  save(strip,'bite-stages.png');
  const animationDir=process.env.PIZZA_FRAME_DIR;
  if(animationDir){
    fs.mkdirSync(animationDir,{recursive:true});
    for(let i=0;i<=60;i++){
      Pizza.draw(clock.getContext('2d'),art,'12:45','12:46',i*60);
      fs.writeFileSync(path.join(animationDir,`frame-${String(i).padStart(3,'0')}.png`),clock.toBuffer('image/png'));
    }
  }
  fs.writeFileSync(path.join(__dirname,'manifest.json'),JSON.stringify({status:'approved',size:[480,320],duration_ms:Pizza.DURATION,slice_size:31,source_boxes:art.boxes,digit_layouts:Pizza.layouts},null,2)+'\n');
  console.log(JSON.stringify({digits:Pizza.layouts.map(x=>x.length),boxes:art.boxes,preview:'clock, digit sheet, bite stages, and animation frames'}));
})().catch(e=>{console.error(e);process.exit(1)});
