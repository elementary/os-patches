import os.path
from apport.hookutils import *

def attach_command_output(report, command_list, key):
	log = command_output(command_list)
	if not log or log[:5] == "Error":
		return
	report[key] = log 

def add_info(report):
	if not apport.packaging.is_distro_package(report['Package'].split()[0]):
		report['ThirdParty'] = 'True'
		report['CrashDB'] = 'indicator_sound'

	if not 'StackTrace' in report:
		attach_command_output(report, ['gdbus', 'call', '--session', '--dest', 'com.canonical.indicator.sound', '--object-path', '/com/canonical/indicator/sound', '--method', 'org.gtk.Actions.DescribeAll'], 'ActionStates')
