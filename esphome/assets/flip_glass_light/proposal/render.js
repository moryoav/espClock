/* Preview only. Shared by the browser simulation and PNG export. */
(function (root) {
  function prepare(makeCanvas, sheet, background) {
    const src = makeCanvas(sheet.width, sheet.height), s = src.getContext('2d');
    s.drawImage(sheet, 0, 0);
    const rgba = s.getImageData(0, 0, src.width, src.height).data;
    const glyphs = [], boxes = [];
    for (let n = 0; n < 11; n++) {
      const col = n % 6, row = Math.floor(n / 6);
      const edges=row?[0,268,508,766,1024,1280,1536]:[0,256,512,768,1024,1280,1536];
      const seen=new Uint8Array(src.width*512), components=[];
      for (let y = row * 512; y < (row + 1) * 512; y++) {
        for (let x = edges[col]; x < edges[col+1]; x++) {
          const local=(y-row*512)*src.width+x;
          if(seen[local] || rgba[(y*src.width+x)*4+3]<32) continue;
          const queue=[[x,y]], box=[x,y,x,y];let count=0;seen[local]=1;
          for(let q=0;q<queue.length;q++) {
            const [a,b]=queue[q];count++;
            box[0]=Math.min(box[0],a);box[1]=Math.min(box[1],b);box[2]=Math.max(box[2],a);box[3]=Math.max(box[3],b);
            for(const [dx,dy] of [[-1,0],[1,0],[0,-1],[0,1]]) {
              const ax=a+dx,by=b+dy,idx=(by-row*512)*src.width+ax;
              if(ax<edges[col] || ax>=edges[col+1] || by<row*512 || by>=(row+1)*512 || seen[idx] || rgba[(by*src.width+ax)*4+3]<32)continue;
              seen[idx]=1;queue.push([ax,by]);
            }
          }
          components.push({count,box});
        }
      }
      components.sort((a,b)=>b.count-a.count);
      const selected=components.slice(0,n===10?2:1);
      let x0=Math.min(...selected.map(c=>c.box[0])),y0=Math.min(...selected.map(c=>c.box[1]));
      let x1=Math.max(...selected.map(c=>c.box[2])),y1=Math.max(...selected.map(c=>c.box[3]));
      if (x1 <= x0 || y1 <= y0) throw Error('Missing glyph ' + n);
      boxes.push([x0, y0, x1 + 1, y1 + 1]);
      const cell = makeCanvas(100, 128), c = cell.getContext('2d');
      const h = n === 10 ? 80 : 126;
      const w = Math.min(n === 10 ? 28 : 98, Math.round((x1-x0+1)*h/(y1-y0+1)));
      c.drawImage(sheet, x0, y0, x1-x0+1, y1-y0+1, (100-w)/2, (128-h)/2, w, h);
      glyphs.push(cell);
    }
    const bg = makeCanvas(480, 320);
    bg.getContext('2d').drawImage(background, 0, 0, 480, 320);
    return { glyphs, bg, boxes, makeCanvas };
  }
  function values(time) { return time.replace(':', '').split('').map(Number); }
  function draw(ctx, art, before='12:45', after=before, p=1, quantize=true) {
    const { makeCanvas, glyphs, bg } = art;
    ctx.clearRect(0,0,480,320); ctx.drawImage(bg,0,0);
    const layer = makeCanvas(480, 320), c = layer.getContext('2d');
    const old = values(before), next = values(after), xs=[14,122,258,366], top=84, half=64;
    function part(g, x, bottom) {
      c.drawImage(glyphs[g],0,bottom?half:0,100,half,x,top+(bottom?half:0),100,half);
    }
    function flap(g,x,bottom,extent,motion) {
      const h=Math.max(1,Math.round(half*extent));
      for(let r=0;r<h;r++) {
        const d=(r+.5)/h, sd=Math.min(half-1,Math.floor(d*half));
        const sy=bottom?half+sd:half-1-sd, dy=top+half+(bottom?r:-1-r);
        const scale=1-.07*motion*d, inset=Math.floor(100*(1-scale)/2);
        c.drawImage(glyphs[g],0,sy,100,1,x+inset,dy,100-2*inset,1);
      }
    }
    for(let i=0;i<4;i++) {
      const x=xs[i];
      if(old[i]===next[i] || p>=1) { c.drawImage(glyphs[next[i]],x,top); continue; }
      if(p<=0) { c.drawImage(glyphs[old[i]],x,top); continue; }
      part(next[i],x,false); part(old[i],x,true);
      if(p<.5) { const a=p*Math.PI; flap(old[i],x,false,Math.cos(a),Math.sin(a)); }
      else { const t=(p-.5)*2, e=1-Math.pow(1-t,3), ex=Math.sin(e*Math.PI/2); flap(next[i],x,true,ex,1-ex); }
    }
    c.drawImage(glyphs[10],190,top);
    // Reflections follow the animated transparent glyph layer, not the room.
    const reflected=makeCanvas(480,320), r=reflected.getContext('2d');
    for(let y=0;y<100;y++) {
      r.globalAlpha=.44*Math.pow(1-y/100,1.25);
      r.drawImage(layer,0,211-Math.floor(y*128/100),480,1,0,215+y,480,1);
    }
    ctx.save(); ctx.filter='blur(1px)'; ctx.drawImage(reflected,0,0); ctx.restore();
    ctx.save(); ctx.filter='blur(2px)'; ctx.fillStyle='rgba(71,93,118,.17)';
    for(const x of xs) { ctx.beginPath();ctx.ellipse(x+50,213,35,2,0,0,Math.PI*2);ctx.fill(); }
    ctx.restore();ctx.drawImage(layer,0,0);
    if(quantize) {
      const frame=ctx.getImageData(0,0,480,320), data=frame.data;
      for(let i=0;i<data.length;i+=4) {
        data[i]=Math.floor((data[i]>>3)*255/31);
        data[i+1]=Math.floor((data[i+1]>>2)*255/63);
        data[i+2]=Math.floor((data[i+2]>>3)*255/31);
      }
      ctx.putImageData(frame,0,0);
    }
  }
  root.LightPreview={prepare,draw};
  if(typeof module!=='undefined') module.exports=root.LightPreview;
})(typeof globalThis!=='undefined'?globalThis:this);
