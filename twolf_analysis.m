%% twolf branch prediction analysis

results = readtable("results\twolf.txt");
results(1:10, :) = [];
%%
results.MATCH = (results.TAGE == 1 & results.ALT == 1 & results.PERCEPTRON >= 0) | ...
    (results.TAGE == 0 & results.ALT == 0 & results.PERCEPTRON < 0);
figure(1)
scatter(results.CONFIDENCE(results.MATCH), results.PERCEPTRON(results.MATCH), 'magenta');
hold on;
scatter(results.CONFIDENCE(~results.MATCH), results.PERCEPTRON(~results.MATCH), 'blue');