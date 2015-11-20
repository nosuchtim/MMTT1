from OpenGL.GL import *
from OpenGL.GLUT import *
from OpenGL.GLU import *
from math import *

window = 0                                             # glut window number
width, height = 500, 400                               # window size
First = True

Cosine = {}
Sine = {}
for angle in range(0,361,1):
	anglerad = pi * angle / 180.0
	Cosine[angle] = cos(anglerad)
	Sine[angle] = sin(anglerad)

def draw_corner(x, y, radius, ang0, ang1):
	x0 = x + (1.0 - Sine[ang0]) * radius
	y0 = y - (1.0 - Cosine[ang0]) * radius
	global First
	for angle in range(ang0,ang1,2):
		x1 = x + (1.0 - Sine[angle]) * radius
		y1 = y - (1.0 - Cosine[angle]) * radius
		glVertex2f(x0, y0)
		glVertex2f(x1, y1)
		x0 = x1
		y0 = y1
	First = False

def draw_rect(x, y, width, height, radius, filled = False):

	# print("draw_rect ",x,y,width,height,radius)
	if filled:
		glBegin(GL_POLYGON)
	else:
		glBegin(GL_LINES)

	# left side
	glVertex2f(x, y+radius)
	glVertex2f(x, y+height-radius)

	# upper left corner
	draw_corner(x, y+height, radius, 0, 91)

	# top side
	glVertex2f(x+radius, y+height)
	glVertex2f(x+width-radius, y+height)

	# upper right corner
	draw_corner(x+width-2*radius, y+height, radius, 270, 361)

	# right side
	glVertex2f(x+width, y+radius)
	glVertex2f(x+width, y+height-radius)

	# lower right corner
	draw_corner(x+width-2*radius, y+2*radius, radius, 180, 271)

	# bottom side
	glVertex2f(x+radius, y)
	glVertex2f(x+width-radius, y)

	# lower left corner
	draw_corner(x, y+2*radius, radius, 90, 181)

	glEnd()  

def draw():             # ondraw is called all the time
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT) # clear the screen
	glLoadIdentity()                                   # reset position
    
	glColor4f(1.0,0.0,0.0,0.7)

	left_xa = -0.725   # left-most holes
	left_xb = -0.475   # second left-most holes
	right_xa = 0.525   # right-most holes
	right_xb = 0.275   # second right-most holes

	top_ya = 0.45
	top_yb = 0.175
	bottom_ya = -0.675
	bottom_yb = -.375

	boxwidth = 0.2
	boxheight = 0.2
	corner = 0.05
	draw_rect(left_xa,top_yb,boxwidth,boxheight,corner, True)
	draw_rect(left_xb,top_yb,boxwidth,boxheight,corner)
	draw_rect(left_xb,top_ya,boxwidth,boxheight,corner)

	draw_rect(left_xa,bottom_yb,boxwidth,boxheight,corner)
	draw_rect(left_xb,bottom_yb,boxwidth,boxheight,corner)
	draw_rect(left_xb,bottom_ya,boxwidth,boxheight,corner)

	draw_rect(right_xa,top_yb,boxwidth,boxheight,corner)
	draw_rect(right_xb,top_yb,boxwidth,boxheight,corner)
	draw_rect(right_xb,top_ya,boxwidth,boxheight,corner)

	draw_rect(right_xa,bottom_yb,boxwidth,boxheight,corner)
	draw_rect(right_xb,bottom_yb,boxwidth,boxheight,corner)
	draw_rect(right_xb,bottom_ya,boxwidth,boxheight,corner)

	from Image import open

	glEnable(GL_BLEND)
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)

	# pos = self.pos
	# NOTE: the return value of glPrint is the x width of the text
	# self.font.glPrint(pos[0],pos[1],self.text)

	glLineWidth(2.0)
	glColor4f(1.0,0.0,0.0,0.7)

	posx, posy = 0,-0.02
	sides = 32
	sz = 0.9
	# glScalef(1.0,0.61803,1.0)
	glScalef(1.0,0.96,1.0)
	glBegin(GL_LINE_LOOP)
	for i in range(sides):
		cosine=sz*cos(i*2*pi/sides)+posx
		sine=sz*sin(i*2*pi/sides)+posy
		glVertex2f(cosine,sine)
	glEnd()

	glutSwapBuffers()


# initialization
glutInit()                                             # initialize glut
glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_ALPHA | GLUT_DEPTH)
glutInitWindowSize(width, height)                      # set window size
glutInitWindowPosition(0, 0)                           # set window position
window = glutCreateWindow("noobtuts.com")              # create window with title
glutDisplayFunc(draw)                                  # set draw function callback
glutIdleFunc(draw)                                     # draw all the time
glutMainLoop()                                         # start everything
