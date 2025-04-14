import os
import jinja2
import IDLParser
import argparse
import sys

def main() :
    if getattr(sys, 'frozen', False):
        base_path = os.path.dirname(sys.executable)  
    else:
        base_path = os.path.dirname(os.path.realpath(__file__))

    arg_parser = argparse.ArgumentParser(description = 'PacketGenerator')
    arg_parser.add_argument('--output', type=str, default='PacketHandler', help='output file')
    arg_parser.add_argument('--enum', type=str, default='CommonProtocol', help='enum output file')
    args = arg_parser.parse_args()

    parser = IDLParser.IDLParser()
    parser.IDL_Parse(base_path +"\Packet.IDL")
    file_loader = jinja2.FileSystemLoader('Templates')
    env = jinja2.Environment(loader=file_loader)
    template = env.get_template('PacketHandler.h')
    output = template.render(parser=parser, output=args.output)

    f = open(args.output+'.h', 'w+')
    f.write(output)
    f.close()

    print(output)

    template = env.get_template('CommonProtocol.h')
    output = template.render(parser=parser)
    f = open(args.enum + '.h', 'w+')
    f.write(output)
    f.close()

    print(output)

if __name__ == "__main__":
    main()
