# Calculation for probability of two devices that can communicate directly
from scipy.integrate import quad
import numpy as np
import math

def f(x):
	return (2.0*math.acos(0.5*x) - x*math.sqrt(1.0-0.25*x*x))/math.pi

def g(x):
	return 2.0*x*f(x)

result = quad(g, 0, 1)

print f(1), f(0.5), f(0), result
	
