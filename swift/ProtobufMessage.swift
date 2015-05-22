//
//  ProtobufMessage.swift
//  ProtocolBuffers_OSX
//
//  Created by David Paschich on 5/21/15.
//
//

import Foundation

protocol Message {
    var bytes: [UInt8] { get }
    var serializedSize: Int { get }
    var unknownFields: [Int:[WireValue]] { get }
    init?([UInt8])
}

protocol Builder {
    typealias M: Message

    var isValid: Bool { get }
    func addTag(tag: Int, value: WireValue)
    func build() -> M?
}
