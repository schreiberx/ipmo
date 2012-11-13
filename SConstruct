#! /usr/bin/python

import os
import commands
import re
import sys
import copy

env = Environment(ENV = os.environ)

env.Append(CPPPATH = ['.', 'client_omp', 'client_tbb', 'client_mpi_tbb', 'include', 'server'])

#
# boolean values should be set to 'on' or 'off' strings via the command line
# True and False values are then set to the configuration variable stored to the env[] array
#

def setupBoolOption(varname, default):
	global env

	env[varname] = GetOption(varname)

	if env[varname]==None:
		env[varname] = default
		return

	if env[varname] not in set(['on', 'off']):
		print 'Invalid option \'"'+env[varname]+'"\' for ' + varname + ' specified'
		Exit(1)

	if env[varname]=='on':
		env[varname] = True
	else:
		env[varname] = False


def setupStringOption(varname, constraints, default):
	global env

	env[varname] = GetOption(varname)

	if env[varname]==None:
		env[varname] = default
	elif env[varname] not in constraints:
		print 'Invalid option \'"'+env[varname]+'"\' for ' + varname + ' specified'
		Exit(1)



################################################################
# COMPILER STUFF
################################################################

#
# compiler (gnu/intel)
#
compiler_constraints = ['gnu', 'intel']
AddOption(	'--compiler',
		dest='compiler',
		type='string',
		nargs=1,
		action='store',
		help='specify compiler to use (gnu/intel), default: gnu')

setupStringOption('compiler', compiler_constraints, 'gnu')

#
# compile mode (debug/release)
#
mode_constraints = ['debug', 'release']
AddOption(	'--mode',
		dest='mode',
		type='string',
		nargs=1,
		action='store',
		help='specify release or debug mode (release/debug), default: release')

setupStringOption('mode', mode_constraints, 'debug')


#
# compile mode (omp/tbb/mpi_tbb)
#
client_constraints = ['none', 'omp', 'tbb', 'mpi_tbb']
AddOption(	'--client',
		dest='client',
		type='string',
		nargs=1,
		action='store',
		help='specify client to compile (none/omp/tbb/mpi_tbb), default: omp')

setupStringOption('client', client_constraints, 'omp')


################################################################
# MIC
################################################################

#
# MIC (true/false)
#
AddOption(	'--enable-mic',		dest='enable_mic',		type='string',		action='store',		help='Build for MIC architecture. default: off')
setupBoolOption('enable_mic', False)



############################
# BUILD BINARIES
############################


if env['compiler'] == 'gnu':
	env.Append(CXXFLAGS=' -std=c++0x')

elif env['compiler'] == 'intel':
	env.Append(CXXFLAGS=' -std=c++0x')

	# SSE 4.2
	if env['enable_mic']:
		env.Append(CXXFLAGS=' -mmic')
		env.Append(LINKFLAGS=' -mmic')


if env['mode'] == 'debug':
	env.Append(CXXFLAGS=' -DDEBUG=1')

	if env['compiler'] == 'gnu':
		env.Append(CXXFLAGS=' -O0 -g3 -Wall')

	elif env['compiler'] == 'intel':
		env.Append(CXXFLAGS=' -O0 -g')

elif env['mode'] == 'release':
	env.Append(CXXFLAGS=' -DNDEBUG=1')

	if env['compiler'] == 'gnu':
		env.Append(CXXFLAGS=' -O3 -mtune=native')

	elif env['compiler'] == 'intel':
		env.Append(CXXFLAGS=' -O3 -fast -fno-alias')



################################################################################################
# SERVER
################################################################################################

server_program_name = "server_ipmo"

# mode
server_program_name += '_'+env['mode']

print
print 'Building server program "'+server_program_name+'"'
print

server_env = env.Clone()

if env['compiler'] == 'gnu':
	server_env.Append(CXXFLAGS=' -fopenmp')
	server_env.Append(LINKFLAGS=' -fopenmp')
	server_env.Replace(CXX = 'g++')

elif env['compiler'] == 'intel':
	server_env.Append(CXXFLAGS=' -openmp')
	server_env.Append(LINKFLAGS=' -openmp')
	server_env.Replace(CXX = 'icpc')


############################
# build directory
#

server_build_dir='build/build_'+server_program_name

############################
# source files
#

server_env.src_files = []

Export('server_env')
server_env.SConscript('server/SConscript', variant_dir=server_build_dir, duplicate=0)
Import('server_env')


############################
# build program
#

server_env.Program('build/'+server_program_name, server_env.src_files)





################################################################################################
# OMP CLIENT
################################################################################################

if env['client'] == 'omp':

	client_omp_program_name = "client_omp"

	# mode
	client_omp_program_name += '_'+env['mode']

	print
	print 'Building client program "'+client_omp_program_name+'"'
	print

	client_omp_env = env.Clone()


	if client_omp_env['compiler'] == 'gnu':
		client_omp_env.Append(CXXFLAGS=' -fopenmp')
		client_omp_env.Append(LINKFLAGS=' -fopenmp')
		client_omp_env.Replace(CXX = 'g++')

	elif client_omp_env['compiler'] == 'intel':
		client_omp_env.Append(CXXFLAGS=' -openmp')
		client_omp_env.Append(LINKFLAGS=' -openmp')
		client_omp_env.Replace(CXX = 'icpc')



	############################
	# build directory
	#

	client_omp_build_dir='build/build_'+client_omp_program_name

	############################
	# source files
	#

	client_omp_env.src_files = []

	Export('client_omp_env')
	client_omp_env.SConscript('client_omp/SConscript', variant_dir=client_omp_build_dir, duplicate=0)
	Import('client_omp_env')


	############################
	# build program
	#

	client_omp_env.Program('build/'+client_omp_program_name, client_omp_env.src_files)




################################################################################################
# TBB CLIENT
################################################################################################

if env['client'] == 'tbb':

	client_tbb_program_name = "client_tbb"

	# mode
	client_tbb_program_name += '_'+env['mode']

	print
	print 'Building client program "'+client_tbb_program_name+'"'
	print

	client_tbb_env = env.Clone()


	if client_tbb_env['compiler'] == 'gnu':
		client_tbb_env.Replace(CXX = 'g++')

	elif client_tbb_env['compiler'] == 'intel':
		client_tbb_env.Replace(CXX = 'icpc')

	client_tbb_env.Append(CXXFLAGS=' -DCOMPILE_WITH_TBB=1')
	client_tbb_env.Append(CXXFLAGS=' -I'+os.environ['TBBROOT']+'/include/')

	client_tbb_env.Append(LIBPATH=[os.environ['TBBROOT']+'/lib/'])

	if 'LD_LIBRARY_PATH' in os.environ:
		client_tbb_env.Append(LIBPATH=os.environ['LD_LIBRARY_PATH'].split(':'))

	if env['mode'] == 'debug':
		client_tbb_env.Append(LIBS=['tbb_debug'])
	else:
		client_tbb_env.Append(LIBS=['tbb'])


	############################
	# build directory
	#

	client_tbb_build_dir='build/build_'+client_tbb_program_name


	############################
	# source files
	#

	client_tbb_env.src_files = []

	Export('client_tbb_env')
	client_tbb_env.SConscript('client_tbb/SConscript', variant_dir=client_tbb_build_dir, duplicate=0)
	Import('client_tbb_env')


	############################
	# build program
	#

	client_tbb_env.Program('build/'+client_tbb_program_name, client_tbb_env.src_files)







################################################################################################
# MPI/TBB CLIENT
################################################################################################

if env['client'] == 'mpi_tbb':

	client_mpi_tbb_program_name = "client_mpi_tbb"

	# mode
	client_mpi_tbb_program_name += '_'+env['mode']

	print
	print 'Building client program "'+client_mpi_tbb_program_name+'"'
	print

	client_mpi_tbb_env = env.Clone()


	if client_mpi_tbb_env['compiler'] == 'gnu':
		client_mpi_tbb_env.Replace(CXX = 'mpic++')
		client_mpi_tbb_env.Append(CXXFLAGS=' -fopenmp')
		client_mpi_tbb_env.Append(LINKFLAGS=' -fopenmp')

	elif client_mpi_tbb_env['compiler'] == 'intel':
		print "Warning: Intel compiler with MPI not tested so far"
		client_mpi_tbb_env.Replace(CXX = 'mpic++')
		client_mpi_tbb_env.Append(CXXFLAGS=' -openmp')
		client_mpi_tbb_env.Append(LINKFLAGS=' -openmp')
		client_mpi_tbb_env.Replace(CXX = 'icpc')

	client_mpi_tbb_env.Append(CXXFLAGS=' -DCOMPILE_WITH_TBB=1')
	client_mpi_tbb_env.Append(CXXFLAGS=' -I'+os.environ['TBBROOT']+'/include/')


	client_mpi_tbb_env.Append(LIBPATH=[os.environ['TBBROOT']+'/lib/'])

	if os.environ['LD_LIBRARY_PATH'] != None:
		client_mpi_tbb_env.Append(LIBPATH=os.environ['LD_LIBRARY_PATH'].split(':'))

	if env['mode'] == 'debug':
		client_mpi_tbb_env.Append(LIBS=['tbb_debug'])
	else:
		client_mpi_tbb_env.Append(LIBS=['tbb'])


	############################
	# build directory
	#

	client_mpi_tbb_build_dir='build/build_'+client_mpi_tbb_program_name


	############################
	# source files
	#

	client_mpi_tbb_env.src_files = []

	Export('client_mpi_tbb_env')
	client_mpi_tbb_env.SConscript('client_mpi_tbb/SConscript', variant_dir=client_mpi_tbb_build_dir, duplicate=0)
	Import('client_mpi_tbb_env')


	############################
	# build program
	#

	client_mpi_tbb_env.Program('build/'+client_mpi_tbb_program_name, client_mpi_tbb_env.src_files)
