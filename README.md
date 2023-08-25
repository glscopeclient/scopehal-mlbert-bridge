# SCPI server for MultiLANE BERTs

MultiLANE BERTs are controlled via Ethernet using a proprietary, undocumented binary protocol.

While the vendor provides a SDK for interfacing with this protocol, they do not appear to have
cross platform client capability as the SDK is only available for Windows.

This application runs on a Windows PC that has network access to the BERT, and exposes a SCPI-style
server allowing remote control of the BERT from any OS.