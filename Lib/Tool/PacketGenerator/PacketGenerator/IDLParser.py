import re

class IDLParser :
    def __init__(self):
        self.m_Protocols = []

    def IDL_Parse(self, path):
        file = open(path, 'r')
        datas = file.read()

        pattern_packet = r'([A-Za-z0-9_]+)\s*:\s*(\d+)\s*{([^}]*)}'
        packets = re.findall(pattern_packet, datas, flags=re.DOTALL)

        for packet in packets:
            packet_name, packet_id_str, fields_str = packet
            packet_id = int(packet_id_str)

            field_pattern = r'([\w\[\], ]+?)\s+(\w+)\s*;'
            fields = re.findall(field_pattern, fields_str)
            fields = [(ftype.strip(), fname.strip()) for ftype, fname in fields]
            
            self.m_Protocols.append({
                "packet_name": packet_name,
                "packet_id": packet_id,
                "fields": fields
            })



        file.close()