#!/usr/bin/python2

from xml.dom.minidom import *
import string
import sys

prefix = '/apps/gnome-control-center-1.6/'
domain = ''

name_map = {'ulong' : 'unsigned long'}
tckind_map = {'ulong' : '5'}
natives_map = {'string' : 'string', 'long' : 'int', 'double' : 'float', 'boolean' : 'bool'}

def totext(nodelist):
	return string.join(map(lambda node: node.toxml(), nodelist), '')
    
class GConfSchema (Element):
	def __init__ (self, key, value, etype = 'string', is_native = 0):
		Element.__init__ (self, 'schema')
		if is_native:
			val_str = value
			etype = natives_map[etype]
		else:
			val_str = '%CORBA:ANY%<?xml version=\"1.0\"?>'
			if type (value) == NodeList:
				val_str = val_str + totext (value)
			else:
				val_str = val_str + value.toxml ()

		nodes = (('key', '/schemas' + prefix + domain + '/' + key),
			 ('applyto', prefix + domain + '/' + key),
			 ('owner', 'gnome-control-center-1.6_' + domain),
			 ('type', etype),
			 ('default', val_str))

		for n in nodes:
			node = Element (n[0])
			node.appendChild (Text (n[1]))
			self.appendChild (node)
		
		node = Element ('locale')
		node.setAttribute ('name', 'C')
		node.appendChild (Element ('short'))
		node.appendChild (Element ('long'))
		self.appendChild (node)

def convertNode (node):
	name = node.getAttribute ('name')
	if node.hasAttribute ('type'):
		type = node.getAttribute ('type')
		value = node.getAttribute ('value')
		
		if natives_map.has_key (type):
			return GConfSchema (name, value, type, 1)
		
		tree = Element ('any')
		
		type_node = Element ('type')
		type_node.setAttribute ('name', name_map[type])
		type_node.setAttribute ('repo_id', '')
		type_node.setAttribute ('tckind', tckind_map[type])
		type_node.setAttribute ('length', '0')
		type_node.setAttribute ('subparts', '0')
		tree.appendChild (type_node)

		value_node = Element ('value')
		value_node.appendChild (Text (value))
		tree.appendChild (value_node)
	else:
		tree = node.childNodes
	
	return GConfSchema (name, tree)
	
if len (sys.argv) < 2:
	sys.exit (1)

for i in range (1, len (sys.argv)):
	filename = sys.argv[i]
	domain = filename[0:string.rindex (filename, '.xml')]
	document = xml.dom.minidom.parse (filename)

	schemas_doc = Document ()
	schemas_root = Element ('gconfschemafile')
	schemas_doc.appendChild (schemas_root)
	schemas = Element ('schemalist')
	schemas_root.appendChild (schemas)

	for node in document.documentElement.childNodes:
		if node.nodeType == node.ELEMENT_NODE and node.nodeName == 'section':
			for entry in node.childNodes:
				if entry.nodeType == entry.ELEMENT_NODE and entry.nodeName == 'entry':
					schemas.appendChild (convertNode (entry))

	out = open ('gnome-control-center-1.6_' + domain + '.schemas', 'w')
	schemas_doc.writexml (out)

