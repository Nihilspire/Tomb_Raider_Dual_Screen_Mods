package com.jack.tombraider;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RectF;
import android.view.View;

/**
 * The tab on the game's right edge that slides Link in and out on one screen (0.18.0, after Shantae Dual's and Xenogears Dual's
 * panels: 26 x 44 on a 320-high screen, faint at 0.55 when shut, 0.85 open). Drawn in Link's colours: the panel's dark brown, a
 * gold edge and a gold arrow (pointing left to bring Link in, right to put it away).
 */
final class PanelArrow extends View {
    private float t;                                         // 0 shut .. 1 open
    private final Paint fill = new Paint(Paint.ANTI_ALIAS_FLAG), edge = new Paint(Paint.ANTI_ALIAS_FLAG), gold = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF r = new RectF();
    private final Path p = new Path();

    PanelArrow(Context c) {
        super(c);
        fill.setColor(0xE60E0B08);
        edge.setColor(0xFFE2BF6A); edge.setStyle(Paint.Style.STROKE);
        gold.setColor(0xFFE2BF6A);
        setContentDescription("Tomb Raider Link");
        setOpen(0);
    }

    void setOpen(float t) { this.t = t; setAlpha(0.55f + 0.30f * t); invalidate(); }

    @Override protected void onDraw(Canvas c) {
        float w = getWidth(), h = getHeight(), k = Math.max(1f, w / 26f), rad = 4 * k;
        edge.setStrokeWidth(Math.max(1f, k));
        r.set(edge.getStrokeWidth() / 2, edge.getStrokeWidth() / 2, w + rad, h - edge.getStrokeWidth() / 2);   // square on the screen edge side
        c.drawRoundRect(r, rad, rad, fill);
        c.drawRoundRect(r, rad, rad, edge);
        float cx = w / 2 + k, cy = h / 2, s = 6 * k, dir = t > 0.5f ? 1 : -1;
        p.reset(); p.moveTo(cx + dir * s * 0.6f, cy); p.lineTo(cx - dir * s * 0.5f, cy - s); p.lineTo(cx - dir * s * 0.5f, cy + s); p.close();
        c.drawPath(p, gold);
    }
}
