import pygame
import sys
import threading
import time
from pygame.locals import *
 
EVENT_TEXT=USEREVENT+1


def parse_line(text):
    ret = []
    while text:
        end = text.find(',')
        if end == -1:
            end = len(text)
        num = int(text[0:end])
        ret.append(num)
        text = text[end+1:]
    return ret


class App:
    def __init__(self):
        self._running = True
        self._display_surf = None
        self.size = self.width, self.height = 640*4, 400
        self.channels = []
        self.dirty = True

    def on_init(self):
        pygame.init()
        self._display_surf = pygame.display.set_mode(self.size, pygame.HWSURFACE | pygame.DOUBLEBUF)
        self._running = True
 
    def on_event(self, event):
        if event.type == pygame.QUIT:
            self._running = False
        elif event.type == EVENT_TEXT:
            try:
                self.channels = parse_line(event.message)
                self.dirty = True
            except:
                pass
    
    def on_loop(self):
        pass

    def on_render(self):
        if not self.dirty:
            return
        w = self.width / 20
        positions = len(self.channels)/3
        for pos in range(0,int(positions)):
            r,g,b = self.channels[pos*3+0],self.channels[pos*3+1],self.channels[pos*3+2]
            self._display_surf.fill(Color(r,g,b),Rect(pos*w,0,w-1,self.height))
        pygame.display.update()
        self.dirty = False

    
    def on_cleanup(self):
        pygame.quit()
 
    def on_execute(self):
        if self.on_init() == False:
            self._running = False
 
#        stdinThread = StdinThread(name='input thread')
#        stdinThread.start()

        f = sys.stdin
        while( self._running ):
            for event in pygame.event.get():
                self.on_event(event)
            self.on_loop()
            self.on_render()
            line = f.readline().strip()
            if line:
                text_event = pygame.event.Event(EVENT_TEXT, message=line)
                pygame.event.clear(EVENT_TEXT)
                pygame.event.post(text_event)
            else:
                time.sleep(1.0/60.0)
        self.on_cleanup()
 

class StdinThread(threading.Thread):
    def run(self):
        with open("fifo") as f:
        # with sys.stdin as f:
            for line in f:
                line = line.strip()
                if line:
                    text_event = pygame.event.Event(EVENT_TEXT, message=line)
                    pygame.event.clear(EVENT_TEXT)
                    pygame.event.post(text_event)


if __name__ == "__main__" :
    theApp = App()
    theApp.on_execute()
