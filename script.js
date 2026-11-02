const grammarInput = document.getElementById('grammarInput');
const testInput = document.getElementById('testInput');
const statusBox = document.getElementById('status');
const output = document.getElementById('output');
const outTitle = document.getElementById('outTitle');
const btnParse = document.getElementById('btnParse');

const allButtons = ['btnLR', 'btnLF', 'btnFirst', 'btnFollow', 'btnTable'].map(function (id) {
  return document.getElementById(id);
});

let analysis = null;       // sections of the backend text for the current grammar
let analysisGrammar = '';

function setStatus(text, cls) {
  statusBox.textContent = text;
  statusBox.className = 'status' + (cls ? ' ' + cls : '');
}

function show(title, text) {
  outTitle.textContent = title;
  output.textContent = text;
  output.scrollTop = 0;
}

async function callBackend(grammar, input) {
  const response = await fetch('/process', {
    method: 'POST',
    headers: { 'Content-Type': 'text/plain; charset=utf-8' },
    body: grammar + '\n%%INPUT%%\n' + input
  });
  if (!response.ok) {
    throw new Error('Server returned HTTP ' + response.status);
  }
  return await response.text();
}


function backendDown(error) {
  setStatus('Backend unavailable. Run main.exe and reload this page.', 'error');
  show('Output', 'Could not connect to the C++ backend.\n\nRun main.exe from the project folder, then open http://localhost:8080 in your browser.\n\nDetails: ' + error.message);
}

function between(text, startMarker, endMarker) {
  const s = text.indexOf(startMarker);
  if (s === -1) return '';
  const from = s + startMarker.length;
  const e = endMarker ? text.indexOf(endMarker, from) : -1;
  return (e === -1 ? text.slice(from) : text.slice(from, e)).replace(/^\n+/, '').replace(/\s+$/, '');
}

function sections(text) {
  return {
    original: between(text, 'ORIGINAL GRAMMAR\n', 'AFTER LEFT RECURSION ELIMINATION\n'),
    afterLR: between(text, 'AFTER LEFT RECURSION ELIMINATION\n', 'AFTER LEFT FACTORING (final grammar)\n'),
    afterLF: between(text, 'AFTER LEFT FACTORING (final grammar)\n', 'NON-TERMINALS\n'),
    nonTerminals: between(text, 'NON-TERMINALS\n', 'TERMINALS\n'),
    terminals: between(text, '\nTERMINALS\n', 'FIRST SETS\n'),
    first: between(text, 'FIRST SETS\n', 'FOLLOW SETS\n'),
    follow: between(text, 'FOLLOW SETS\n', 'LL(1) PARSING TABLE\n'),
    table: between(text, 'LL(1) PARSING TABLE\n', 'INPUT PARSING TRACE\n'),
    trace: between(text, 'INPUT PARSING TRACE\n', null).replace(/^-+\n/, '')
  };
}

// Makes sure the grammar has been analysed. Returns true when analysis is ready.
async function ensureAnalysis() {
  const grammar = grammarInput.value.trim();
  if (!grammar) {
    setStatus('Enter a grammar first.', 'error');
    show('Output', 'Enter a grammar first.');
    return false;
  }
  if (analysis && analysisGrammar === grammar) {
    return true;
  }

  setStatus('Analyzing grammar…');
  let text;
  try {
    text = await callBackend(grammar, '');
  } catch (error) {
    backendDown(error);
    return false;
  }
  if (text.startsWith('GRAMMAR ERROR')) {
    analysis = null;
    show('Grammar error', text.trim());
    setStatus('Grammar validation failed. Read the error in the output window.', 'error');
    return false;
  }
  analysis = sections(text);
  analysisGrammar = grammar;
  return true;
}

const steps = {
  btnLR: function () {
    show('Left Recursion Elimination',
      'ORIGINAL GRAMMAR\n' + analysis.original +
      '\n\nAFTER LEFT RECURSION ELIMINATION\n' + analysis.afterLR);
    setStatus('Showing left recursion elimination.', 'success');
  },
  btnLF: function () {
    show('Left Factoring', 'AFTER LEFT FACTORING (final grammar)\n' + analysis.afterLF);
    setStatus('Showing left factoring.', 'success');
  },
  btnFirst: function () {
    show('FIRST sets',
      'NON-TERMINALS\n' + analysis.nonTerminals +
      '\n\nTERMINALS\n' + analysis.terminals +
      '\n\nFIRST SETS\n' + analysis.first);
    setStatus('Showing FIRST sets.', 'success');
  },
  btnFollow: function () {
    show('FOLLOW sets', 'FOLLOW SETS\n' + analysis.follow);
    setStatus('Showing FOLLOW sets.', 'success');
  },
  btnTable: function () {
    show('LL(1) Parsing Table', 'LL(1) PARSING TABLE\n' + analysis.table);
    if (analysis.table.includes('LL(1) STATUS: NO')) {
      setStatus('Table has conflicts. This grammar is not LL(1).', 'warning');
    } else {
      setStatus('Showing the LL(1) parsing table.', 'success');
    }
  }
};

allButtons.forEach(function (btn) {
  btn.addEventListener('click', async function () {
    btn.disabled = true;
    try {
      if (await ensureAnalysis()) steps[btn.id]();
    } finally {
      btn.disabled = false;
    }
  });
});

btnParse.addEventListener('click', async function () {
  btnParse.disabled = true;
  btnParse.textContent = 'Parsing…';
  try {
    if (!(await ensureAnalysis())) return;
    const text = await callBackend(analysisGrammar, testInput.value);
    const trace = sections(text).trace;
    show('Input Parsing Trace', 'INPUT PARSING TRACE\n' + trace);
    if (trace.includes('Result: ACCEPTED')) {
      setStatus('String accepted.', 'success');
    } else if (trace.includes('Result: REJECTED')) {
      setStatus('String rejected. See the trace in the output window.', 'warning');
    } else if (trace.includes('Parsing skipped')) {
      setStatus('Parsing skipped: the grammar is not LL(1).', 'warning');
    } else {
      setStatus('Done.');
    }
  } catch (error) {
    backendDown(error);
  } finally {
    btnParse.disabled = false;
    btnParse.innerHTML = 'Parse String <span aria-hidden="true">→</span>';
  }
});

document.getElementById('exampleBtn').addEventListener('click', function () {
  grammarInput.value = 'E -> E+T | T\nT -> T*F | F\nF -> (E) | id';
  testInput.value = 'id+id*id';
  setStatus('Example loaded. Press any step button.');
});
