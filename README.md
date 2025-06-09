# StePS JANUS - STEreographically Projected cosmological Simulations in the JANUS cosmological model

Note that this code is a work in progress as to how one can fully integrate the Janus model with the tools provided by the Master StePS repository as most available code for running such simulations does not take into account negative masses as well as visualization and analisys tools.For example most sph tools just won't be able to deal with negative masses.

Some tools made available here will allow to extract negative masses points and data in order to separately make density, speed etc... analisys.

## An N-body code for non-periodic JANUS cosmological simulations

We present a novel N-body simulation method that compactifies the infinite spatial extent of the Universe into a finite sphere with isotropic boundary conditions to follow the evolution of the large-scale structure. Our approach eliminates the need for periodic boundary conditions, a mere numerical convenience which is not supported by observation and which modifies the law of force on large scales in an unrealistic fashion. With this code, it is possible to simulate an infinite universe with unprecedented dynamic range for a given amount of memory, and in contrast of the traditional periodic simulations, its fundamental geometry and topology match observations.

The StePS code is optimized to run on GPU accelerated HPC systems.

Janus Simulation:


[![YouTube](http://i.ytimg.com/vi/t1iGr5U2Hhs/hqdefault.jpg)](https://www.youtube.com/watch?v=t1iGr5U2Hhs)


For more information see: [astro-ph](https://arxiv.org/abs/1711.04959) and [astro-ph](https://arxiv.org/abs/1811.05903)

If you plan to publish an academic paper using this software, please consider citing the following publication:

G. Rácz, I. Szapudi, I. Csabai, L. Dobos, "Compactified Cosmological Simulations of the Infinite Universe": MNRAS, Volume 477, Issue 2, p.1949-1957
