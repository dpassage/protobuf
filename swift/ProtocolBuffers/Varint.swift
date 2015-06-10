//
//  Varint.swift
//  ProtocolBuffers
//
//  Created by David Paschich on 6/9/15.
//  Copyright © 2015 David Paschich. All rights reserved.
//

import Foundation

public enum VarintError: ErrorType {
    case ParseFailed
}

public struct Varint {
    let bytes: [UInt8]

    init(value: UInt64) {
        var _bytes: [UInt8] = []
        var _value = value
        while (true) {
            if ((_value & ~0x7F) == 0) {
                _bytes.append(UInt8(_value))
                break
            } else {
                _bytes.append(UInt8((_value & 0x7F) | 0x80));
                _value = _value >> 7
            }
        }
        bytes = _bytes
    }

    init(value: UInt32) {
        self.init(value: UInt64(value))
    }

    init(value: Int) {
        self.init(value: UInt64(value))
    }

//    init(tag: Int, type: WireType) {
//        self.init(value: UInt64(tag) << 3 | UInt64(type.rawValue))
//    }

    init(bytes: [UInt8]) {
        self.bytes = bytes
    }

    init(value: Bool) {
        self.bytes = value ? [1] : [0]
    }

    // if successful, returns a tuple of a new Varint followed by
    // the number of bytes consumed.
    public static func fromBytes(bytes: [UInt8]) throws -> (Varint, Int) {
        for i in 0...10 {
            if i >= bytes.count {
                throw VarintError.ParseFailed
            }
            if bytes[i] & 0x80 == 0 {
                let varint = Varint(bytes: [UInt8](bytes[0...i]))
                return (varint, varint.bytes.count)
            }
        }
        throw VarintError.ParseFailed
    }

    func as_bool() -> Bool {
        switch bytes.count {
        case 0:
            return false
        default:
            return (bytes[0] & 1) == 1
        }
    }
}
