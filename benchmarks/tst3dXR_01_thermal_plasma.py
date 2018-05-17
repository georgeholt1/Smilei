# ----------------------------------------------------------------------------------------
# 					SIMULATION PARAMETERS FOR THE PIC-CODE SMILEI
# ----------------------------------------------------------------------------------------

import math as m


TkeV = 10.						# electron & ion temperature in keV
T   = TkeV/511.   				# electron & ion temperature in me c^2
n0  = 1.
Lde = m.sqrt(T)					# Debye length in units of c/\omega_{pe}
dx  = 0.5*Lde 					# cell length (same in x & y)
dy  = dx
dz  = dx
dt  = 0.95 * dx/m.sqrt(3.)		# timestep (0.95 x CFL)

Lx    = 32.*dx
Ly    = 32.*dy
Lz    = 32.*dz
Tsim  = 2.*m.pi			

def n0_(x,y,z):
	if (0.1*Lx<x<0.9*Lx) and (0.1*Ly<y<0.9*Ly) and (0.1*Lz<z<0.9*Lz):
		return n0
	else:
		return 0.


Main(
    geometry = "3drz",
    nmodes = 2,
    solve_poisson = False,
    
    interpolation_order = 2,
    
    timestep = dt,
    simulation_time = Tsim,
    
    cell_length  = [dx,dy],
    grid_length = [Lx,Ly],
    
    number_of_patches = [1,1],
    
    EM_boundary_conditions = [
        ["silver-muller","silver-muller"],
        ["buneman","buneman"],
    ],
    
    print_every = 1,

    random_seed = smilei_mpi_rank
)


#LoadBalancing(
#    every = 20,
#    cell_load = 1.,
#    frozen_particle_load = 0.1
#)


#Species(
#    name = "proton",
#    position_initialization = "random",
#    momentum_initialization = "maxwell-juettner",
#    particles_per_cell = 1, 
#    c_part_max = 1.0,
#    mass = 1836.0,
#    charge = 1.0,
#    charge_density = n0,
#    mean_velocity = [0., 0.0, 0.0],
#    temperature = [T],
#    pusher = "boris",
#    boundary_conditions = [
#    	["remove", "remove"],
#    	["remove", "remove"],
#    ],
#)
Species(
    name = "electron",
    position_initialization = "random",
    momentum_initialization = "maxwell-juettner",
    particles_per_cell = 1, 
    c_part_max = 1.0,
    mass = 1.0,
    charge = -1.0,
    charge_density = n0,
    mean_velocity = [0., 0.0, 0.0],
    temperature = [T],
    pusher = "boris",
    boundary_conditions = [
    	["remove", "remove"],
    	["remove", "remove"],
    ],
)

#Checkpoints(
#    dump_step = 0,
#    dump_minutes = 0.0,
#    exit_after_dump = False,
#)

DiagFields(
    every = 20,
    fields = ["Br_m_mode_0", "Br_m_mode_1","Bx_m_mode_0","Bx_m_mode_1","Bt_m_mode_0","Bt_m_mode_1","Bt_mode_0","Bt_mode_1","Bx_mode_0","Bx_mode_1","Br_mode_0","Br_mode_1","Er_mode_0","Er_mode_1","Et_mode_0","Et_mode_1","Ex_mode_0","Ex_mode_1","Rho_mode_0", "Rho_mode_1", "Jx_mode_0", "Jx_mode_1", "Jl_mode_0", "Jl_mode_1", "Jt_mode_0", "Jt_mode_1" ]
)

#DiagScalar(every = 1)

#for direction in ["forward", "backward", "both", "canceling"]:
#	DiagScreen(
#	    shape = "sphere",
#	    point = [0., Ly/2., Lz/2.],
#	    vector = [Lx*0.9, 0.1, 0.1],
#	    direction = direction,
#	    deposited_quantity = "weight",
#	    species = ["electron"],
#	    axes = [
#	    	["theta", 0, math.pi, 10],
#	    	["phi", -math.pi, math.pi, 10],
#	    	],
#	    every = 40,
#	    time_average = 30
#	)
#	DiagScreen(
#	    shape = "plane",
#	    point = [Lx*0.9, Ly/2., Lz/2.],
#	    vector = [1., 0.1, 0.1],
#	    direction = direction,
#	    deposited_quantity = "weight",
#	    species = ["electron"],
#	    axes = [
#	    	["a", -Ly/2., Ly/2., 10],
#	    	["b", -Lz/2., Lz/2., 10],
#	    	],
#	    every = 40,
#	    time_average = 30
#	)


