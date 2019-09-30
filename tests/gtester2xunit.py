#! /usr/bin/python
from argparse import ArgumentParser
import libxslt
import libxml2
import sys
import os

XSL_TRANSFORM='/usr/share/gtester2xunit/gtester.xsl'

def transform_file(input_filename, output_filename, xsl_file):
    gtester = libxml2.parseFile(xsl_file)
    style = libxslt.parseStylesheetDoc(gtester)
    doc = libxml2.parseFile(input_filename)
    result = style.applyStylesheet(doc, None)
    result.saveFormatFile(filename=output_filename, format=True)


def get_output_filename(input_filename):
    filename, extension = os.path.splitext(input_filename)
    return '{filename}-xunit{extension}'.format(filename=filename,
                                                 extension=extension)

def main():
    parser = ArgumentParser(
            description="Simple utility that converts xml output of " +
                        "gtester to xunit output for jenkins")
    parser.add_argument('-o', '--output',
            help="Write output to specified file instead of default")
    parser.add_argument('-x', '--xsl',
            help="xsl file that should be used for the transformation")
    parser.add_argument('-i', '--in-place',
            help="Write the ouput to the original file (i.e. replace " +
            "the original xml)",
            action='store_true',
            default=False)
    parser.add_argument('files', metavar='FILE', type=str, nargs='+',
            help="file(s) to transform to xunit format")
    args = vars(parser.parse_args())
    for input_filename in args['files']:
        if not args['output']:
            if args['in_place']:
                output_filename = input_filename
            else:
                output_filename = get_output_filename(input_filename)
        else:
            output_filename = args['output']
        if not args['xsl']:
            xsl = XSL_TRANSFORM
        else:
            xsl = args['xsl']
        transform_file(input_filename, output_filename, xsl)

sys.exit(main())
# vim: set syntex=python:
