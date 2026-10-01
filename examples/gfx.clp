<< @clpp.gfx showcase: shapes, text, gradients and a CPU "shader" effect, saved to a PNG.
<< Runs headless (CLPP_HEADLESS set), so it needs no window — great for generating images.
link @clpp.gfx as Gfx;
link @clpp.axiom as Axiom;

Gfx.Canvas(480, 300);
Gfx.Gradient(0, 0, 480, 300, 0x1B1033, 0x0C0C16);

<< a ring of circles, colored around the hue wheel
for (let i in 12) {
  let angle = Axiom.Rad(i * 30);
  let x = 240 + Axiom.Cos(angle) * 90;
  let y = 150 + Axiom.Sin(angle) * 90;
  Gfx.Circle(x, y, 16, Gfx.Hsv(i * 30, 0.7, 1));
}

Gfx.RoundRect(160, 110, 160, 80, 16, Gfx.Fade(0x7C5CFF, 0.9));
Gfx.Text("CL++", 198, 132, 32, Gfx.WHITE);
Gfx.Effect("bloom", 0.7);
Gfx.Effect("vignette", 0.5);

post("saved: " .: Gfx.Save("showcase.png"));
post("size: " .: Gfx.Width() .: "x" .: Gfx.Height());
