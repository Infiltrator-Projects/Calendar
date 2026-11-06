#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build Calendar's application-only manual with semantic figures and page checks.

Source: docs/USER-GUIDE.md. Separate Calendar-* / Clock-* resources own their own
manuals. The font search prefers InfiltratorOS's verified MB Corpo resources;
--allow-preview-fallback is solely for local review on hosts without them.
"""
from __future__ import annotations

import argparse
import re
from pathlib import Path
from xml.sax.saxutils import escape

from reportlab.lib import colors
from reportlab.lib.enums import TA_LEFT
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.units import mm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.pdfgen.canvas import Canvas
from reportlab.platypus import (BaseDocTemplate, Flowable, Frame, KeepTogether,
    PageBreak, PageTemplate, Paragraph, Spacer, Table, TableStyle)

INK = colors.HexColor('#26323E')
NAVY = colors.HexColor('#102033')
BLUE = colors.HexColor('#00ADEF')
PALE = colors.HexColor('#EDF6FA')
MUTED = colors.HexColor('#657482')
LINE = colors.HexColor('#C9D9E4')
PAGE_W, PAGE_H = A4
MARGIN = 18*mm
CONTENT_W = PAGE_W - 2*MARGIN
FONT_DIRS = [Path('/System/Resources/Fonts'), Path('/usr/local/share/fonts'), Path.home()/'.local/share/fonts']


def fonts(allow_fallback: bool):
    files = {}
    for directory in FONT_DIRS:
        if directory.is_dir():
            for file in directory.rglob('*.ttf'):
                files.setdefault(file.name.lower(), file)
    mb = ['mb_corpo_s_title_regular.ttf', 'mb_corpo_s_title_bold.ttf',
          'mb_corpo_a_title_cond_regular.ttf']
    # The verified Common baseline uses these three MB Corpo 1.010 Title faces.
    if all(key in files for key in mb):
        pdfmetrics.registerFont(TTFont('Body', str(files[mb[0]])))
        pdfmetrics.registerFont(TTFont('Body-Bold', str(files[mb[1]])))
        pdfmetrics.registerFont(TTFont('Brand', str(files[mb[2]])))
        pdfmetrics.registerFontFamily('Body', normal='Body', bold='Body-Bold')
        return True
    if not allow_fallback:
        raise RuntimeError('The verified MB Corpo font resources are required. '
                           'Install them under /System/Resources/Fonts; '
                           'use --allow-preview-fallback only for non-release previews.')
    pdfmetrics.registerFont(TTFont('Body','/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'))
    pdfmetrics.registerFont(TTFont('Body-Bold','/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'))
    pdfmetrics.registerFont(TTFont('Brand','/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'))
    pdfmetrics.registerFontFamily('Body', normal='Body', bold='Body-Bold')
    return False


def styles():
    return {
      'cover': ParagraphStyle('cover',fontName='Brand',fontSize=43,leading=48,textColor=NAVY,spaceAfter=12),
      'sub': ParagraphStyle('sub',fontName='Body',fontSize=12.5,leading=20,textColor=MUTED,spaceAfter=14),
      'kicker': ParagraphStyle('kicker',fontName='Body-Bold',fontSize=9,leading=13,textColor=BLUE,spaceAfter=8),
      'h': ParagraphStyle('h',fontName='Body-Bold',fontSize=22,leading=28,textColor=NAVY,spaceAfter=17),
      'body': ParagraphStyle('body',fontName='Body',fontSize=10.0,leading=15.5,textColor=INK,spaceAfter=10),
      'bullet': ParagraphStyle('bullet',fontName='Body',fontSize=9.8,leading=15,textColor=INK,leftIndent=14,firstLineIndent=-10,spaceAfter=7),
      'tiny': ParagraphStyle('tiny',fontName='Body',fontSize=8.0,leading=12,textColor=MUTED,spaceAfter=6),
      'toc': ParagraphStyle('toc',fontName='Body',fontSize=10.5,leading=21,textColor=NAVY,spaceAfter=5),
    }


def rich(s):
    s = escape(s)
    s = re.sub(r'\*\*(.+?)\*\*',r'<b>\1</b>',s)
    s = re.sub(r'`([^`]+)`',r'<font color="#087FA8">\1</font>',s)
    return s


class Diagram(Flowable):
    """Actual application relationships from the Cinnamon sources; NOT screenshot art."""
    def __init__(self, name):
        super().__init__(); self.name=name
        self.width=CONTENT_W; self.height={'popup':180,'settings':145,'navigation':94,'events':94,'ownership':130}[name]
    def draw(self):
        c=self.canv; w=self.width; h=self.height
        c.setFillColor(PALE); c.setStrokeColor(LINE)
        c.roundRect(0,0,w,h,9,stroke=1,fill=1)
        def box(x,y,bw,bh,label,light=False):
            c.setFillColor(colors.white if light else NAVY)
            c.roundRect(x,y,bw,bh,5,stroke=0,fill=1)
            c.setFillColor(NAVY if light else colors.white)
            c.setFont('Body-Bold',8.5)
            c.drawCentredString(x+bw/2,y+bh/2-3,label)
        if self.name=='popup':
            box(14,h-40,110,25,'A  PANEL CLOCK')
            c.setFillColor(MUTED);c.setFont('Body',8);c.drawString(132,h-31,'select to open popup')
            box(14,40,178,93,'B  EVENTS / AGENDA',True)
            for i,txt in enumerate(('selected date','09:00   Meeting','13:30   Appointment')):
                c.setFillColor(INK);c.setFont('Body',8);c.drawString(25,111-i*21,txt)
            box(204,103,w-218,30,'C  TODAY HEADING')
            box(204,70,w-218,29,'D  MONTH / YEAR')
            box(204,40,w-218,26,'E  DATE GRID')
            box(14,9,w-28,25,'F  DATE & TIME SETTINGS     |     ABOUT',True)
        elif self.name=='settings':
            c.setFillColor(NAVY);c.setFont('Body-Bold',11);c.drawString(15,h-24,'Calendar applet configuration')
            fields=[('Display / Theme','Follow system  /  Day  /  Night'),('Show calendar events','On by default'),('Show week numbers','Off by default'),('Show calendar','Super+C (changeable)')]
            for i,(a,b) in enumerate(fields):
                y=h-52-i*26
                c.setStrokeColor(LINE);c.line(15,y-7,w-15,y-7)
                c.setFillColor(INK);c.setFont('Body-Bold',8.5);c.drawString(17,y+2,a)
                c.setFont('Body',8);c.setFillColor(MUTED);c.drawRightString(w-17,y+2,b)
        elif self.name=='navigation':
            labs=[('<<  /  >>','YEAR'),('<  /  >','MONTH'),('DAY CELL','SELECT'),('TODAY','RESET')]
            bw=(w-30-24)/4
            for i,(txt,lab) in enumerate(labs):
                x=15+i*(bw+8);box(x,40,bw,28,txt)
                c.setFillColor(MUTED);c.setFont('Body',7.5);c.drawCentredString(x+bw/2,23,lab)
        elif self.name=='events':
            labs=['DESKTOP CALENDAR','CINNAMON EVENTS','CALENDAR AGENDA']
            bw=(w-52)/3
            for i,lab in enumerate(labs):
                x=15+i*(bw+11);box(x,33,bw,35,lab,light=i==1)
                if i<2:
                    c.setFillColor(BLUE);c.setFont('Body-Bold',12);c.drawCentredString(x+bw+5,45,'>')
            c.setFillColor(MUTED);c.setFont('Body',8);c.drawCentredString(w/2,13,'Calendar displays; editing takes place in the calendar application')
        elif self.name=='ownership':
            bw=(w-52)/3
            for i,(label,lower) in enumerate([('CALENDAR APPLET','Theme / events / weeks / key'),('CINNAMON','Locale / time zone / week start'),('SHARED SETTINGS','Optional installed providers')]):
                x=15+i*(bw+11);box(x,60,bw,48,label,light=i==1)
                c.setFillColor(MUTED);c.setFont('Body',7.7);c.drawCentredString(x+bw/2,37,lower)
            c.setFillColor(NAVY);c.setFont('Body-Bold',8.6)
            c.drawCentredString(w/2,12,'Calendar consumes resource modules; it does not install them')


class Manual(BaseDocTemplate):
    def __init__(self, path, release_fonts):
        super().__init__(str(path),pagesize=A4,leftMargin=MARGIN,rightMargin=MARGIN,topMargin=22*mm,bottomMargin=18*mm,
                         title='Calendar - User Manual',author='Shannon Smith',subject='Calendar Cinnamon panel applet operating manual')
        self.release_fonts=release_fonts
        fr=Frame(MARGIN,18*mm,CONTENT_W,PAGE_H-40*mm,leftPadding=0,rightPadding=0,topPadding=0,bottomPadding=0)
        self.addPageTemplates(PageTemplate(id='main',frames=fr,onPage=self.page))
    def page(self,c,doc):
        c.saveState()
        c.setFillColor(NAVY);c.rect(0,PAGE_H-13*mm,PAGE_W,13*mm,stroke=0,fill=1)
        c.setFillColor(BLUE);c.rect(MARGIN,PAGE_H-13*mm,24*mm,2.4,stroke=0,fill=1)
        c.setFont('Body-Bold',8);c.setFillColor(colors.white)
        c.drawString(MARGIN,PAGE_H-8.4*mm,'CALENDAR  /  USER MANUAL')
        c.setStrokeColor(LINE);c.line(MARGIN,13.4*mm,PAGE_W-MARGIN,13.4*mm)
        c.setFillColor(MUTED);c.setFont('Body',7.5)
        c.drawString(MARGIN,9.0*mm,'Infiltrator Projects  |  Calendar application only')
        c.drawRightString(PAGE_W-MARGIN,9.0*mm,str(doc.page))
        c.restoreState()


def generate(src, dest, preview=False):
    release_fonts=fonts(allow_fallback=preview)
    sty=styles(); lines=src.read_text(encoding='utf-8').splitlines()
    if not lines or lines[0]!='# Calendar':raise ValueError('Expected Calendar guide')
    story=[Spacer(1,28*mm),Paragraph('INFILTRATOR PROJECTS  /  APPLICATION MANUAL',sty['kicker']),
           Paragraph('Calendar',sty['cover']),Paragraph('Native clock and calendar for Cinnamon',sty['sub']),
           Spacer(1,4*mm),Diagram('popup'),Spacer(1,12*mm),
           Paragraph('A working guide to the panel applet, popup, date navigation, events and configuration.',sty['sub']),
           Spacer(1,12*mm),Paragraph('<b>SCOPE:</b> Application controls only. Separately installed clock and calendar resources have their own manuals.',sty['body']),
           Spacer(1,10*mm),Paragraph('Copyright 1993-2026 Shannon Smith  |  GPL-3.0-or-later',sty['tiny']),PageBreak(),
           Paragraph('Contents',sty['h'])]
    heads=[('1. Getting started','3'),('2. Understanding the popup','4'),('3. Moving through dates','5'),
           ('4. Calendar applet settings','6'),('5. Events and the desktop calendar','7'),
           ('6. System time, alternative resources and location','8'),('7. Troubleshooting','9'),
           ('8. Quick reference and boundaries','10')]
    for name,page in heads:
        story.append(Paragraph(f'{escape(name)} <font color="#657482"> . . . . . . . . . . . . {page}</font>',sty['toc']))
    story.extend([Spacer(1,12*mm),Paragraph('Scope of this manual',sty['h']),
                  Paragraph('This manual documents the actual Calendar applet and how it integrates with Cinnamon. It deliberately does not duplicate individual calendar-system or clock-mode reference material.',sty['body']),
                  Paragraph('The interface illustrations are labelled schematics, not screenshots. They reflect the application layout and settings schema; exact appearance follows the installed Cinnamon version and theme.',sty['body'])])
    for raw in lines[1:]:
        t=raw.strip()
        if not t or t.startswith('Native Cinnamon panel clock'):continue
        if t=='<!-- PAGE -->':story.append(PageBreak());continue
        m=re.fullmatch(r'<!-- DIAGRAM:(\w+) -->',t)
        if m:story.extend([Diagram(m.group(1)),Spacer(1,9*mm)]);continue
        if t.startswith('## '):story.append(Paragraph(rich(t[3:]),sty['h']));continue
        if t.startswith('- '):story.append(Paragraph('&bull; '+rich(t[2:]),sty['bullet']));continue
        if re.match(r'^\d+\. ',t):story.append(Paragraph(rich(t),sty['bullet']));continue
        if t=='---':story.append(Spacer(1,5*mm));continue
        story.append(Paragraph(rich(t),sty['body']))
    dest.parent.mkdir(parents=True,exist_ok=True)
    Manual(dest,release_fonts).build(story)
    from pypdf import PdfReader
    pdf=PdfReader(str(dest))
    if len(pdf.pages)<10:raise AssertionError(f'Incomplete guide: {len(pdf.pages)} pages')
    content='\n'.join(p.extract_text() or '' for p in pdf.pages)
    for item in ('Getting started','Understanding the popup','Calendar applet settings',
                 'System time, alternative resources and location','Troubleshooting','No Events'):
        if item not in content:raise AssertionError(f'Missing manual section {item}')
    print(f'{dest} | {len(pdf.pages)} pages | MB Corpo embedded: {release_fonts} | {dest.stat().st_size} bytes')

if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('source',type=Path)
    parser.add_argument('destination',type=Path)
    parser.add_argument('--allow-preview-fallback',action='store_true')
    a=parser.parse_args()
    generate(a.source,a.destination,a.allow_preview_fallback)