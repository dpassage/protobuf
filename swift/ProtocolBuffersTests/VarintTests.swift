//
//  VarintTests.swift
//  ProtocolBuffers
//
//  Created by David Paschich on 6/9/15.
//  Copyright © 2015 David Paschich. All rights reserved.
//

import XCTest
import ProtocolBuffers

class VarintTests: XCTestCase {

    override func setUp() {
        super.setUp()
        // Put setup code here. This method is called before the invocation of each test method in the class.
    }

    override func tearDown() {
        // Put teardown code here. This method is called after the invocation of each test method in the class.
        super.tearDown()
    }

    func testTooShortVarintThrows() {
        let inputBytes: [UInt8] = [0x80]

        do {
            _ = try Varint.fromBytes(inputBytes)
        } catch {
            // test passed, return
            return
        }
        XCTFail("parse should have vailed")
    }

    func testSingleByteInt() {
        let inputBytes: [UInt8] = [0x01]

        do {
            let (_, bytesConsumed) = try Varint.fromBytes(inputBytes)
            XCTAssertEqual(bytesConsumed, 1)

        } catch {
            XCTFail("should not have thrown")
        }

    }

    func testTooManyBytes() {
        let inputBytes = Array<UInt8>(count: 11, repeatedValue: 0x80)

        do {
            _ = try Varint.fromBytes(inputBytes)
        } catch {
            // test passed, return
            return
        }
        XCTFail("parse should have vailed")
    }
    func testPerformanceExample() {
    }

}
