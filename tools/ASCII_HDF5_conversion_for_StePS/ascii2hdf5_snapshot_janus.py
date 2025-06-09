#!/usr/bin/env python3

#*******************************************************************************#
#  ascii2hdf5_snapshot_janus.py - An ASCII to hdf5 file converter for StePS           #
#     (STEreographically Projected cosmological Simulations) snapshots.         #
#    Copyright (C) 2017-2022 Gabor Racz                                         #
#                                                                               #
#    This program is free software; you can redistribute it and/or modify       #
#    it under the terms of the GNU General Public License as published by       #
#    the Free Software Foundation; either version 2 of the License, or          #
#    (at your option) any later version.                                        #
#                                                                               #
#    This program is distributed in the hope that it will be useful,            #
#    but WITHOUT ANY WARRANTY; without even the implied warranty of             #
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              #
#    GNU General Public License for more details.                               #
#*******************************************************************************#


#this script creates an hdf5 file in which positive masses and negative masses are in seperate datasets within a single hdf5 file.
import numpy as np
import h5py
import sys
import time
# %matplotlib inline

#Beginning of the script
if len(sys.argv) != 4:
    print("Error:")
    print("usage: ./ascii2hdf5_snapshot.py <input ASCII snapshot> <output HDF5 snapshot> <precision 0: 32bit 1: 64bit>\nExiting.")
    sys.exit(2)

if int(sys.argv[3]) != 0 and int(sys.argv[3]) != 1:
    print("Error:")
    print("Unkown output precision.\nExiting.")
    sys.exit(2)

print("Reading the input ASCII file...")
start = time.time()


ASCII_snapshot=np.fromfile(str(sys.argv[1]), count=-1, sep='\t', dtype=np.float64)
ASCII_snapshot = ASCII_snapshot.reshape(int(len(ASCII_snapshot)/7),7)

N=len(ASCII_snapshot)

posmass=np.where(ASCII_snapshot[:, 6, None] > 0, ASCII_snapshot, 0)
negmass=np.where(ASCII_snapshot[:, 6, None] < 0, ASCII_snapshot, 0)
posmass=posmass[~np.all(posmass==0, axis=1)]
negmass=negmass[~np.all(negmass==0, axis=1)]
N_pos=len(posmass)
N_neg=len(negmass)
M_min = np.min(negmass[:,6])
R_max = np.max(np.abs(ASCII_snapshot[:,0:3]))

print("Number of particles:\t%i\nNumber of positive masses:\t%i\nNumber of Negative Masses\t%i\nMinimal mass:\t%f*10e11M_sol\nMaximal radius:\t%fMpc" % (N,N_neg,N_pos,M_min,R_max))




end = time.time()
print("..done in %fs. \n\n" % (end-start))

print("Saving the snapshot in HDF5 format...")
start = time.time()
HDF5_snapshot = h5py.File(str(sys.argv[2]), "w")
#Creating the header
header_group = HDF5_snapshot.create_group("/Header")
#Writing the header attributes
header_group.attrs['NumPart_ThisFile'] = np.array([N_neg,N_pos,0,0,0,0],dtype=np.uint32)
header_group.attrs['NumPart_Total'] = np.array([N_neg,N_pos,0,0,0,0],dtype=np.uint32)
header_group.attrs['NumPart_Total_HighWord'] = np.array([0,0,0,0,0,0],dtype=np.uint32)
header_group.attrs['MassTable'] = np.array([0,0,0,0,0,0],dtype=np.float64)
header_group.attrs['Time'] = np.double(1.0)
header_group.attrs['Redshift'] = np.double(0.0)
header_group.attrs['Lbox'] = np.double(R_max*2.01)
header_group.attrs['NumFilesPerSnapshot'] = int(1)
header_group.attrs['Omega0'] = np.double(1.0)
header_group.attrs['OmegaLambda'] = np.double(0.0)
header_group.attrs['HubbleParam'] = np.double(1.0)
header_group.attrs['Flag_Sfr'] = int(0)
header_group.attrs['Flag_Cooling'] = int(0)
header_group.attrs['Flag_StellarAge'] = int(0)
header_group.attrs['Flag_Metals'] = int(0)
header_group.attrs['Flag_Feedback'] = int(0)
header_group.attrs['Flag_Entropy_ICs'] = int(0)
#Header created.

#Creating datasets for the POSITIVE particle data
particle_group = HDF5_snapshot.create_group("/PartType1")
if int(sys.argv[3]) == 0:
    HDF5datatype = 'float32'
    npdatatype = np.float32
if int(sys.argv[3]) == 1:
    HDF5datatype = 'double'
    npdatatype = np.float64
X = particle_group.create_dataset("Coordinates", (N_pos,3),dtype=HDF5datatype)
V = particle_group.create_dataset("Velocities", (N_pos,3),dtype=HDF5datatype)
IDs = particle_group.create_dataset("ParticleIDs", (N_pos,),dtype='uint64')
M = particle_group.create_dataset("Masses", (N_pos,),dtype=HDF5datatype)

#Saving the particle data
X[:,:] = posmass[:,0:3]
V[:,:] = posmass[:,3:6]
M[:] = posmass[:,6]
IDs[:] = np.arange(N_pos, dtype=np.uint64)


######################################################################################
#Creating datasets for the NEGATIVE particle data
particle_groupN = HDF5_snapshot.create_group("/PartType0")
if int(sys.argv[3]) == 0:
    HDF5datatype = 'float32'
    npdatatype = np.float32
if int(sys.argv[3]) == 1:
    HDF5datatype = 'double'
    npdatatype = np.float64
Xn = particle_groupN.create_dataset("Coordinates", (N_neg,3),dtype=HDF5datatype)
Vn = particle_groupN.create_dataset("Velocities", (N_neg,3),dtype=HDF5datatype)
IDsn = particle_groupN.create_dataset("ParticleIDs", (N_neg,),dtype='uint64')
Mn = particle_groupN.create_dataset("Masses", (N_neg,),dtype=HDF5datatype)

#Saving the particle data
Xn[:,:] = negmass[:,0:3]
Vn[:,:] = negmass[:,3:6]
Mn[:] = negmass[:,6]
IDsn[:] = np.arange(N_neg, dtype=np.uint64)


HDF5_snapshot.close()
end = time.time()




print("..done in %fs. \n\n" % (end-start))
