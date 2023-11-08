# -*- coding: utf-8 -*-

""" Module for converting EWP project format file
    @file
"""

import os
from lxml import objectify


class EWPProject(object):
    """ Class for converting EWP project format file
    """

    def __init__(self, path, xmlFile):
        self.project = {}
        self.path = path
        self.xmlFile = xmlFile
        xmltree = objectify.parse(xmlFile)
        self.root = xmltree.getroot()

    def parseProject(self):
        """ Parses EWP project file for project settings
        """
        self.project['name'] = self.root.configuration.name
        self.project['chip'] = ''

        #TODO: parse into tree structure
        self.project['srcs'] = []
        self.searchGroups(self.root, self.project['srcs'])

        self.project['defs'] = []
        self.project['incs'] = []

        for element in self.root.configuration.getchildren():
            if element.tag == 'settings':
                for e in element.data.getchildren():
                    if e.tag == 'option':
                        if e.name.text == 'OGChipSelectEditMenu':
                            self.project['chip'] = str(e.state)
                        elif e.name.text == 'CCDefines':
                            for d in e.getchildren():
                                if d.tag == 'state' and d.text != None:
                                    self.project['defs'].append(d.text)
                        elif e.name.text == 'CCIncludePath2':
                            for d in e.getchildren():
                                if d.tag == 'state' and d.text != None:
                                    self.project['incs'].append(d.text)

        for i in range(0, len(self.project['incs'])):
            s = str(self.project['incs'][i])
            if os.path.sep not in s:
                if os.path.sep == '\\':
                    s = s.replace('/', '\\')
                elif os.path.sep == '/':
                    s = s.replace('\\', '/')

            self.project['incs'][i] = s.replace('$PROJ_DIR$'+os.path.sep+'..', self.path)

        self.project['files'] = []
        i = 0

        if os.path.exists(self.path + '/Drivers/CMSIS/Device/ST/STM32F3xx/Source/Templates/gcc'):
            for entry in os.listdir(self.path + '/Drivers/CMSIS/Device/ST/STM32F3xx/Source/Templates/gcc'):
                if entry.endswith('.S') or entry.endswith('.s'):
                    self.project['files'].append(self.path + '/Drivers/CMSIS/Device/ST/STM32F3xx/Source/Templates/gcc/'+entry)

    def displaySummary(self):
        """ Display summary of parsed project settings
        """
        print('Project Name:' + self.project['name'])
        print('Project chip:' + self.project['chip'])
        print('Project includes: ' + ' '.join(self.project['incs']))
        print('Project defines: ' + ' '.join(self.project['defs']))
        print('Project srcs: ' + ' '.join(self.project['srcs']))

    def searchGroups(self, xml, sources):
        """ Display summary of parsed project settings
        @param xml XML file with project settings configuration
        @param sources List containing source files
        """
        for element in xml.getchildren():
            if element.tag == 'group':
                self.searchGroups(element, sources)
            elif element.tag == 'file':
                if not str(element.name).endswith('.s'):
                    s = str(element.name)
                    if os.path.sep not in s:
                        if os.path.sep == '\\':
                            s = s.replace('/', '\\')
                        elif os.path.sep == '/':
                            s = s.replace('\\', '/')

                    sources.append(s.replace('$PROJ_DIR$'+os.path.sep+'..', self.path))

    def getProject(self):
        """ Return parsed project settings stored as dictionary
        @return Dictionary containing project settings
        """
        return self.project
