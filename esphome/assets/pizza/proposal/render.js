/* Pizza face design simulation. No firmware or live configuration changes. */
(function(root){
  const SIZE=31, TOP=107, POSITIONS=[53,141,281,366], DURATION=3600;
  const PATHS={
    1:[[['M',9,22],['L',36,1],['L',36,103]]],
    2:[[['M',3,23],['C',4,-5,64,-9,63,26],['C',62,51,8,65,4,103],['L',65,103]]],
    3:[[['M',6,5],['C',72,-10,76,47,34,48],['C',81,48,77,119,5,99]]],
    4:[[['M',51,1],['L',2,69],['L',65,69]],[['M',51,1],['L',51,103]]],
    5:[[['M',63,1],['L',5,1],['L',4,48],['C',76,27,86,117,4,102]]],
    6:[[['M',61,3],['C',17,-9,-3,50,7,83],['C',18,122,69,113,65,77],['C',62,43,4,45,7,83]]],
    7:[[['M',2,1],['L',65,1],['L',24,103]]]
  };
  // Each stroke is sampled into individual overlapping pepperoni slices.
  function stroke(commands){
    let pen=[0,0], poly=[];
    for(const [kind,...p] of commands){
      if(kind==='M'){pen=p;poly.push(p);continue}
      const from=pen;
      if(kind==='L'){poly.push(p);pen=p;continue}
      for(let i=1;i<=80;i++){
        const t=i/80,u=1-t;
        poly.push([u*u*u*from[0]+3*u*u*t*p[0]+3*u*t*t*p[2]+t*t*t*p[4],u*u*u*from[1]+3*u*u*t*p[1]+3*u*t*t*p[3]+t*t*t*p[5]]);
      }
      pen=p.slice(4);
    }
    const distances=[0];
    for(let i=1;i<poly.length;i++) distances.push(distances[i-1]+Math.hypot(poly[i][0]-poly[i-1][0],poly[i][1]-poly[i-1][1]));
    const length=distances.at(-1),count=Math.max(1,Math.round(length/23));let j=1;
    return Array.from({length:count+1},(_,n)=>{
      const d=n*length/count;while(j<distances.length-1&&distances[j]<d)j++;
      const t=(d-distances[j-1])/(distances[j]-distances[j-1]||1);
      return [poly[j-1][0]+(poly[j][0]-poly[j-1][0])*t,poly[j-1][1]+(poly[j][1]-poly[j-1][1])*t];
    });
  }
  function ellipse(cx,cy,rx,ry,count){return Array.from({length:count},(_,i)=>{const a=-Math.PI/2+i*Math.PI*2/count;return[cx+Math.cos(a)*rx,cy+Math.sin(a)*ry]})}
  const layouts=[];
  for(let n=0;n<10;n++){
    let points;
    if(n===0)points=ellipse(32,52,29,51,12);
    else if(n===8)points=[...ellipse(32,25,26,24,7),...ellipse(32,77,29,27,8)];
    else if(n===9)points=layouts[6].map(p=>[64-p[0],104-p[1]]).reverse();
    else points=PATHS[n].flatMap(stroke);
    layouts[n]=points.filter((p,i)=>!points.slice(0,i).some(q=>Math.hypot(p[0]-q[0],p[1]-q[1])<12));
  }
  function bite(c,cx,cy,r){
    c.beginPath();c.arc(cx,cy,r,0,Math.PI*2);c.fill();
    const facing=Math.atan2(32-cy,32-cx);
    for(let i=0;i<5;i++){
      const a=facing+(i-2)*.43;
      c.beginPath();c.arc(cx+Math.cos(a)*r,cy+Math.sin(a)*r,4,0,Math.PI*2);c.fill();
    }
  }
  function prepare(makeCanvas,sheet,background){
    const src=makeCanvas(sheet.width,sheet.height),s=src.getContext('2d');s.drawImage(sheet,0,0);
    const pixels=s.getImageData(0,0,src.width,src.height).data,sprites=[],boxes=[];
    const cw=src.width/3,ch=src.height/2;
    for(let n=0;n<6;n++){
      const col=n%3,row=Math.floor(n/3);let left=src.width,top=src.height,right=0,bottom=0;
      for(let y=row*ch;y<(row+1)*ch;y++)for(let x=col*cw;x<(col+1)*cw;x++){
        if(pixels[(y*src.width+x)*4+3]<32)continue;
        left=Math.min(left,x);top=Math.min(top,y);right=Math.max(right,x);bottom=Math.max(bottom,y);
      }
      if(left>=right||top>=bottom)throw Error('Missing pepperoni slice '+n);
      boxes.push([left,top,right+1,bottom+1]);
      const stages=[];
      for(let phase=0;phase<5;phase++){
        const tile=makeCanvas(64,64),c=tile.getContext('2d');
        if(phase<4){
          c.drawImage(sheet,left,top,right-left+1,bottom-top+1,1,1,62,62);
          c.globalCompositeOperation='destination-out';
          if(phase>=1)bite(c,55,10,23);
          if(phase>=2)bite(c,6,42,26);
          if(phase>=3)bite(c,46,56,25);
        }
        stages.push(tile);
      }
      sprites.push(stages);
    }
    const bg=makeCanvas(480,320);bg.getContext('2d').drawImage(background,0,0,480,320);
    return {makeCanvas,sprites,bg,boxes};
  }
  function piece(ctx,art,x,y,variant,phase=0,placement=1,angle=0){
    if(phase>=4||placement<=0)return;
    const p=Math.min(1,placement),e=1-Math.pow(1-p,3),lift=1-e;
    ctx.save();ctx.globalAlpha=Math.min(1,p*4);
    ctx.translate(x+lift*((variant%3)-1)*7,y-lift*23);ctx.rotate(angle+lift*.16);
    const size=SIZE*(1+lift*.45);
    ctx.shadowColor='rgba(70,20,0,.4)';ctx.shadowBlur=1+lift*3;ctx.shadowOffsetY=1+lift*4;
    ctx.drawImage(art.sprites[variant%6][phase],-size/2,-size/2,size,size);
    ctx.restore();
  }
  function digit(ctx,art,value,x,y,{mode='still',time=0}={}){
    const points=layouts[value];
    points.forEach((p,i)=>{
      const phase=mode==='eat'?Math.max(0,Math.min(4,1+Math.floor((time-110-i*29)/370))):0;
      const placement=mode==='place'?(time-i*65)/440:1;
      piece(ctx,art,x+p[0],y+p[1],(value*7+i*5)%6,phase,placement,((i*17+value*7)%29-14)*Math.PI/180);
    });
  }
  function draw(ctx,art,from='12:45',to=from,ms=DURATION,rgb565=true){
    ctx.clearRect(0,0,480,320);ctx.drawImage(art.bg,0,0);
    const old=from.replace(':','').split('').map(Number),next=to.replace(':','').split('').map(Number);
    for(let i=0;i<4;i++){
      if(old[i]===next[i]||ms>=DURATION)digit(ctx,art,next[i],POSITIONS[i],TOP);
      else if(ms<2000)digit(ctx,art,old[i],POSITIONS[i],TOP,{mode:'eat',time:ms});
      else digit(ctx,art,next[i],POSITIONS[i],TOP,{mode:'place',time:ms-2050});
    }
    piece(ctx,art,239,137,1);piece(ctx,art,239,183,4);
    if(rgb565){
      const frame=ctx.getImageData(0,0,480,320),d=frame.data;
      for(let i=0;i<d.length;i+=4){d[i]=Math.floor((d[i]>>3)*255/31);d[i+1]=Math.floor((d[i+1]>>2)*255/63);d[i+2]=Math.floor((d[i+2]>>3)*255/31)}
      ctx.putImageData(frame,0,0);
    }
  }
  root.PizzaPreview={prepare,draw,digit,piece,layouts,DURATION};
  if(typeof module!=='undefined')module.exports=root.PizzaPreview;
})(typeof globalThis!=='undefined'?globalThis:this);
