/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *
 * @format
 */

'use strict';

const {parse, types, transformFromAstSync} = require('hermes-parser');

describe('Traversal', () => {
  test('Replacement', () => {
    const source = '(foo());';
    const plugin = ({types: t}) => {
      return {
        visitor: {
          CallExpression(path, state) {
            expect(t).toBe(types);
            expect(path.get('callee').node.name).toEqual('foo');
            path.get('callee').replaceWith(t.Identifier('bar'));
          },
        },
      };
    };
    const {ast} = transformFromAstSync(parse(source), source, {
      plugins: [plugin],
    });
    expect(ast).toMatchObject({
      type: 'Program',
      body: [
        {
          type: 'ExpressionStatement',
          expression: {
            type: 'CallExpression',
            callee: {
              type: 'Identifier',
              name: 'bar',
            },
          },
        },
      ],
    });
  });

  test('Accepts options', () => {
    const source = '(foo());';
    let called = false;

    const ast = parse(source);
    transformFromAstSync(ast, source, {
      plugins: [
        [
          {
            visitor: {
              CallExpression(path, state) {
                expect(state.file.ast).toBe(ast);
                expect(state.file.code).toBe(source);
                expect(state.opts.someRandomKey).toBe(true);
                called = true;
              },
            }
          },
          {someRandomKey: true},
        ]
      ],
    });

    expect(called).toBe(true);
  });
});
