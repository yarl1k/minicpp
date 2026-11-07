const examples = {
  factorial: `int factorial(int n) {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}

int main() {
    int sum = 0;
    for (int i = 1; i <= 5; i++) {
        sum = sum + i;
    }

    double ratio = 10.0 / 4.0;
    bool ready = ratio > 2.0 && sum == 15;

    if (ready) {
        return factorial(5);
    } else {
        return 0;
    }
}`,
  functions: `double average(int a, int b) {
    return (a + b) / 2.0;
}

int main() {
    double value = average(7, 12);
    bool passed = value >= 9.0;
    if (passed) {
        return 1;
    }
    return 0;
}`,
  while: `int main() {
    int value = 1;
    int counter = 0;
    while (counter < 6) {
        value = value * 2;
        counter = counter + 1;
    }
    bool isLarge = value >= 64;
    if (isLarge && value != 0) {
        return value % 10;
    } else {
        return 0;
    }
}`,
  void: `void touch(int amount) {
    int local = amount * 2;
    return;
}

int main() {
    touch(10);
    return 7;
}`,
  badsyntax: `int main() {
    int value = 10
    if (value > 5) {
        return value;
    }
}`,
  badsemantic: `int main() {
    int value = 10;
    double ratio = true;
    missing = value + 1;
    return value;
}`,
  print: `int main() {
    int count = 10;
    double price = 3.14;
    bool ready = true;

    print(count);
    print(price);
    print(ready);

    return 0;
}`,
};

const source = document.getElementById('source');
const lineNumbers = document.getElementById('lineNumbers');
const sourceStats = document.getElementById('sourceStats');
const compileBtn = document.getElementById('compileBtn');
let lastPython = '';
let currentSvg = null;
let viewTransform = { x: 0, y: 0, scale: 1 };

function updateEditorMeta() {
  const count = source.value.split('\n').length;
  lineNumbers.textContent = Array.from({length: count}, (_, i) => i + 1).join('\n');
  sourceStats.textContent = `${count} рядків · ${source.value.length} символів`;
}
source.addEventListener('input', updateEditorMeta);
source.addEventListener('scroll', () => { lineNumbers.scrollTop = source.scrollTop; });
source.addEventListener('keydown', event => {
  if (event.key === 'Tab') {
    event.preventDefault();
    const start = source.selectionStart;
    const end = source.selectionEnd;
    source.setRangeText('    ', start, end, 'end');
    updateEditorMeta();
  }
});

document.getElementById('exampleSelect').addEventListener('change', event => {
  source.value = examples[event.target.value];
  updateEditorMeta();
});
document.getElementById('resetBtn').addEventListener('click', () => {
  source.value = examples.factorial;
  document.getElementById('exampleSelect').value = 'factorial';
  updateEditorMeta();
});

document.querySelectorAll('.tab').forEach(button => button.addEventListener('click', () => {
  document.querySelectorAll('.tab').forEach(item => item.classList.toggle('active', item === button));
  document.querySelectorAll('.tab-content').forEach(panel => panel.classList.toggle('active', panel.id === `tab-${button.dataset.tab}`));
}));

function setIndicator(id, state, textId, text) {
  const indicator = document.getElementById(id);
  indicator.className = `status-indicator ${state}`;
  document.getElementById(textId).textContent = text;
}
function showDiagnostics(items, type) {
  const target = document.getElementById('diagnosticsList');
  target.innerHTML = '';
  if (!items.length) {
    target.innerHTML = '<div class="empty-state">Помилок не знайдено.</div>';
    return;
  }
  items.forEach(item => {
    const row = document.createElement('div');
    row.className = `diagnostic ${type}`;
    const tag = document.createElement('span');
    tag.className = 'tag';
    tag.textContent = item.line ? `line ${item.line}` : type === 'syntax' ? 'lexer/parser' : 'semantic';
    const message = document.createElement('span');
    message.textContent = item.message || item;
    row.append(tag, message);
    target.append(row);
  });
}
function setResult(success, title, description) {
  const icon = document.getElementById('resultIcon');
  icon.className = `result-icon ${success === null ? 'neutral' : success ? 'success' : 'failure'}`;
  icon.textContent = success === null ? '⌁' : success ? '✓' : '!';
  document.getElementById('resultTitle').textContent = title;
  document.getElementById('resultDescription').textContent = description;
}

compileBtn.addEventListener('click', async () => {
  compileBtn.disabled = true;
  compileBtn.textContent = 'Аналізуємо…';
  setResult(null, 'Виконується аналіз', 'Lexer передає токени Bison-парсеру, після чого будується AST.');
  try {
    const response = await fetch('/api/compile', {
      method: 'POST',
      headers: {'Content-Type': 'text/plain; charset=utf-8'},
      body: source.value
    });
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    const data = await response.json();
    const syntaxErrors = data.syntaxErrors || [];
    const semanticErrors = data.semanticErrors || [];
    setIndicator('syntaxIndicator', data.parsed ? 'ok' : 'error', 'syntaxStatus', data.parsed ? 'Успішно' : 'Помилка');
    setIndicator('semanticIndicator', data.semanticOk ? 'ok' : data.parsed ? 'error' : 'idle', 'semanticStatus', data.semanticOk ? 'Успішно' : data.parsed ? 'Помилка' : 'Не виконано');
    const runtimeFailed = Boolean(data.runtimeError);
    setIndicator('runIndicator', !data.semanticOk ? 'idle' : runtimeFailed ? 'error' : 'ok', 'runStatus', !data.semanticOk ? 'Не виконано' : runtimeFailed ? 'Помилка' : 'Завершено');

    const all = [
      ...syntaxErrors.map(message => ({message})),
      ...semanticErrors.map(error => ({line: error.line, message: error.message})),
      ...(runtimeFailed ? [{message: data.runtimeError}] : [])
    ];
    showDiagnostics(all, syntaxErrors.length ? 'syntax' : 'semantic');
    if (!data.parsed) setResult(false, 'Синтаксична помилка', 'Парсер не зміг побудувати AST. Перевір структуру програми та повідомлення нижче.');
    else if (!data.semanticOk) setResult(false, 'Семантична помилка', 'Структура програми коректна, але знайдено помилки типів, змінних або викликів функцій.');
    else if (runtimeFailed) setResult(false, 'Помилка виконання', data.runtimeError);
    else setResult(true, 'Компіляцію завершено успішно', data.resultText !== '' ? `Програма виконана. Значення return: ${data.resultText}.` : 'Програма виконана без помилок.');

    if (data.ast) {
      renderAst(data.ast);
      document.getElementById('astCount').textContent = countNodes(data.ast);
    } else {
      document.getElementById('astCanvas').innerHTML = '<div class="empty-state">AST недоступне через синтаксичну помилку.</div>';
      document.getElementById('astCount').textContent = '0';
    }
    lastPython = data.python || '';
    document.querySelector('#pythonOutput code').textContent = lastPython || '# Python-код недоступний через помилки аналізу';
  } catch (error) {
    setIndicator('syntaxIndicator', 'error', 'syntaxStatus', 'Немає з’єднання');
    setIndicator('semanticIndicator', 'idle', 'semanticStatus', 'Не виконано');
    setIndicator('runIndicator', 'idle', 'runStatus', 'Не виконано');
    setResult(false, 'Не вдалося зв’язатися з бекендом', 'Переконайся, що C++ сервер запущений командою minicpp --serve.');
    showDiagnostics([{message: error.message}], 'syntax');
  } finally {
    compileBtn.disabled = false;
    compileBtn.innerHTML = '<span>▶</span> Аналізувати й виконати';
  }
});

function childrenOf(node) {
  const result = [];
  const add = (label, value) => { if (value && typeof value === 'object') result.push({label, node: value}); };
  switch (node.type) {
    case 'Program': (node.functions || []).forEach((item, i) => add(`function ${i + 1}`, item)); break;
    case 'FunctionDeclaration': (node.parameters || []).forEach((item, i) => add(`parameter ${i + 1}`, item)); add('body', node.body); break;
    case 'Block': (node.statements || []).forEach((item, i) => add(`statement ${i + 1}`, item)); break;
    case 'VariableDeclaration': add('initializer', node.initializer); break;
    case 'Assignment': add('value', node.value); break;
    case 'IfStatement': add('condition', node.condition); add('then', node.thenBranch); add('else', node.elseBranch); break;
    case 'WhileStatement': add('condition', node.condition); add('body', node.body); break;
    case 'ForStatement': add('initializer', node.initializer); add('condition', node.condition); add('step', node.step); add('body', node.body); break;
    case 'ReturnStatement': add('value', node.value); break;
    case 'ExpressionStatement': add('expression', node.expression); break;
    case 'UnaryExpression': add('operand', node.operand); break;
    case 'BinaryExpression': add('left', node.left); add('right', node.right); break;
    case 'FunctionCall': (node.arguments || []).forEach((item, i) => add(`argument ${i + 1}`, item)); break;
  }
  return result;
}
function nodeLabel(node) {
  const type = node.type || 'Node';
  const short = type.replace('Statement', '').replace('Expression', 'Expr').replace('Declaration', 'Decl');
  let detail = '';
  if (node.name && (node.returnType || node.declaredType || node.dataType)) {
    detail = `${node.returnType || node.declaredType || node.dataType} ${node.name}`;
  } else if (node.name) detail = node.name;
  else if (node.operator) detail = node.operator;
  else if (node.value !== undefined) detail = String(node.value);
  return detail ? `${short}\n${detail}` : short;
}
function countNodes(node) { return 1 + childrenOf(node).reduce((sum, child) => sum + countNodes(child.node), 0); }
function colorFor(type) {
  if (type.includes('Function')) return ['#27365f','#819aff'];
  if (type.includes('Literal') || type === 'Variable') return ['#21443f','#65d6b0'];
  if (type.includes('Expression') || type === 'UnaryExpression') return ['#44345a','#c19bff'];
  if (type.includes('If') || type.includes('While') || type.includes('For')) return ['#4a3b25','#f0c36c'];
  if (type.includes('Return')) return ['#4d2d3d','#ff91a6'];
  return ['#263851','#8bb9e8'];
}
function renderAst(tree) {
  const canvas = document.getElementById('astCanvas');
  canvas.innerHTML = '';
  const nodes = [];
  const edges = [];
  const leafStep = 205;
  let leafX = 0;
  function layout(node, depth, parent, relation) {
    const children = childrenOf(node);
    const current = {node, depth, x: 0, y: depth * 108, children: []};
    nodes.push(current);
    if (parent) edges.push({parent, child: current, relation});
    if (!children.length) current.x = leafX++ * leafStep;
    else {
      children.forEach(child => current.children.push(layout(child.node, depth + 1, current, child.label)));
      current.x = (current.children[0].x + current.children[current.children.length - 1].x) / 2;
    }
    return current;
  }
  const root = layout(tree, 0, null, '');
  const width = Math.max(520, leafX * leafStep + 150);
  const height = Math.max(300, (Math.max(...nodes.map(n => n.depth)) + 1) * 108 + 55);
  const svgNS = 'http://www.w3.org/2000/svg';
  const svg = document.createElementNS(svgNS, 'svg');
  svg.setAttribute('viewBox', `-80 -30 ${width} ${height}`);
  svg.setAttribute('width', String(width));
  svg.setAttribute('height', String(height));
  const group = document.createElementNS(svgNS, 'g');
  svg.append(group);
  edges.forEach(edge => {
    const line = document.createElementNS(svgNS, 'path');
    const x1 = edge.parent.x, y1 = edge.parent.y + 26, x2 = edge.child.x, y2 = edge.child.y - 26;
    const mid = (y1 + y2) / 2;
    line.setAttribute('d', `M ${x1} ${y1} C ${x1} ${mid}, ${x2} ${mid}, ${x2} ${y2}`);
    line.setAttribute('class', 'ast-edge'); group.append(line);
    const label = document.createElementNS(svgNS, 'text');
    label.setAttribute('x', String((x1 + x2) / 2)); label.setAttribute('y', String(mid - 5));
    label.setAttribute('text-anchor', 'middle'); label.setAttribute('fill', '#8494b1'); label.setAttribute('font-size', '8'); label.textContent = edge.relation;
    group.append(label);
  });
  nodes.forEach(item => {
    const [fill, stroke] = colorFor(item.node.type || '');
    const g = document.createElementNS(svgNS, 'g'); g.setAttribute('class', 'ast-node');
    g.setAttribute('transform', `translate(${item.x - 77},${item.y - 25})`);
    const rect = document.createElementNS(svgNS, 'rect'); rect.setAttribute('width', '154'); rect.setAttribute('height', '50'); rect.setAttribute('fill', fill); rect.setAttribute('stroke', stroke); g.append(rect);
    const lines = nodeLabel(item.node).split('\n');
    const text = document.createElementNS(svgNS, 'text'); text.setAttribute('x', '77'); text.setAttribute('y', lines.length > 1 ? '20' : '29');
    lines.forEach((content, index) => { const tspan = document.createElementNS(svgNS, 'tspan'); tspan.setAttribute('x', '77'); if (index) { tspan.setAttribute('dy', '15'); tspan.setAttribute('class', 'node-line'); } tspan.textContent = content.length > 22 ? `${content.slice(0, 19)}…` : content; text.append(tspan); });
    g.append(text); group.append(g);
  });
  canvas.append(svg);
  currentSvg = {svg, group};
  viewTransform = {x: 0, y: 0, scale: 1};
  enablePanZoom(svg, group);
}
function applyTransform(group) { group.setAttribute('transform', `translate(${viewTransform.x},${viewTransform.y}) scale(${viewTransform.scale})`); }
function enablePanZoom(svg, group) {
  let dragging = false, last = null;
  svg.addEventListener('pointerdown', event => { dragging = true; last = {x: event.clientX, y: event.clientY}; svg.setPointerCapture(event.pointerId); });
  svg.addEventListener('pointermove', event => { if (!dragging || !last) return; viewTransform.x += event.clientX - last.x; viewTransform.y += event.clientY - last.y; last = {x: event.clientX, y: event.clientY}; applyTransform(group); });
  svg.addEventListener('pointerup', () => { dragging = false; last = null; });
  svg.addEventListener('pointercancel', () => { dragging = false; last = null; });
  svg.addEventListener('wheel', event => { event.preventDefault(); const factor = event.deltaY < 0 ? 1.1 : .9; viewTransform.scale = Math.min(2.5, Math.max(.35, viewTransform.scale * factor)); applyTransform(group); }, {passive:false});
}
document.getElementById('resetViewBtn').addEventListener('click', () => {
  if (!currentSvg) return;
  viewTransform = {x: 0, y: 0, scale: 1}; applyTransform(currentSvg.group);
});
document.getElementById('copyPythonBtn').addEventListener('click', async () => {
  if (!lastPython) return;
  try { await navigator.clipboard.writeText(lastPython); document.getElementById('copyPythonBtn').textContent = 'Скопійовано'; setTimeout(() => document.getElementById('copyPythonBtn').textContent = 'Копіювати', 1400); }
  catch { document.getElementById('copyPythonBtn').textContent = 'Не вдалося'; }
});
document.getElementById('downloadPythonBtn').addEventListener('click', () => {
  if (!lastPython) return;
  const blob = new Blob([lastPython], {type:'text/x-python;charset=utf-8'});
  const url = URL.createObjectURL(blob);
  const link = document.createElement('a'); link.href = url; link.download = 'generated.py'; link.click(); URL.revokeObjectURL(url);
});

source.value = examples.factorial;
updateEditorMeta();
