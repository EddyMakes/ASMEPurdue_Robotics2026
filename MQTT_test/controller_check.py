import pygame

pygame.init()
pygame.joystick.init()
print(f"joysticks connected:", pygame.joystick.get_count())
